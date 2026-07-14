/**
Chasing Limit Executor

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2026 Vitezslav Kot <vitezslav.kot@stonky.cz>, Stonky s.r.o.
*/

#ifndef DIRTY_CARRY_CHASING_LIMIT_EXECUTOR_H
#define DIRTY_CARRY_CHASING_LIMIT_EXECUTOR_H

#include <stonky/interface/i_execution_gateway.h>
#include <chrono>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>

namespace stonky::execution {

enum class ExecStatus {
    FILLED, /// full target qty filled inside deadline (within fill tolerance)
    PARTIAL, /// nonzero fills but short of target
    UNFILLED, /// zero fills — deadline reached or benign skip (see note)
    ABORTED /// fatal error (see error)
};

struct ExecutionResult {
    std::string symbol;
    std::optional<OrderSide> side{}; /// nullopt when no order was submitted (sub-min skip, no-quote abort)
    double targetQty{};
    double filledQty{};
    double avgFillPrice{}; /// 0 when no fills
    int numReposts{};
    int numCancels{};
    int numRejects{};
    int numChunksSubmitted{}; /// iceberg only; 0 when unchunked
    double elapsedS{};
    ExecStatus status{ExecStatus::UNFILLED};
    /// Fatal reason (status == ABORTED)
    std::string error{};
    /// Non-fatal explanation of a deliberate skip (sub-min notional, sub-lot
    /// delta) — such a leg was intentionally NOT traded; render as "skipped",
    /// not "failed".
    std::string note{};
    /// Most recent venue reject reason (empty when no order was rejected) —
    /// the diagnostic for UNFILLED-with-rejects legs.
    std::string lastReject{};
};

struct ChasingConfig {
    /// Wait between re-price cycles when no event wakes the loop earlier.
    double repostIntervalS{3.0};
    /// Minimum wall-clock interval between price changes of one resting order.
    /// 0 keeps the legacy behavior: every relevant quote update may re-price
    /// immediately. Venues which implement an amend as cancel + new submit
    /// (MEXC) must set this to a positive value or a fast book creates an
    /// unsustainable cancel/submit storm.
    double minRepriceIntervalS{0.0};
    /// Re-price a resting order only TOWARD the market (chase), never away
    /// from it. A retreating re-price (a buy following the bid down) cancels
    /// the order exactly when its position is most valuable — alone at the
    /// front, first in line for every fill into the dip — and rejoins a lower
    /// level at the BACK of the queue. On fat bot-quoted 1-tick books (MEXC)
    /// queue priority is effectively the whole fill probability, and each
    /// cancel+resubmit on an amend-less venue additionally costs two paced
    /// REST calls and an off-book gap (live-observed: $10 closes UNFILLED for
    /// 5 h straight while the pair traded every ~10 s). The trade-off — an
    /// occasional fill 1-2 ticks past the drifted market — is noise at
    /// chunked-iceberg sizes.
    bool repriceOnlyTowardMarket{false};
    /// 0 = pure passive maker (sit AT best bid/ask). Positive = N ticks INSIDE
    /// the spread, clamped so the price always stays maker-valid.
    int aggressivenessTicks{1};
    /// targetQty * fillTolerance counts as FILLED (absorbs qty-step rounding).
    double fillTolerance{0.999};
    /// A cached quote older than this is treated as missing (WS lag guard).
    double maxQuoteAgeS{10.0};
    /// Iceberg: max chunk size expressed in quote-currency notional, converted
    /// to qty at the live mid per execute() call. 0 = no iceberg (single order).
    double maxChunkNotional{0.0};
    /// Chunk size randomization, fraction [0,1): sample uniform in
    /// [(1-jitter)*chunk, chunk]. Anti-pattern-detection.
    double chunkQtyJitter{0.0};
    /// Random pause [min,max] seconds after a chunk fully fills before the next
    /// chunk submits. 0/0 = submit immediately (true iceberg).
    double chunkDelayMinS{0.0};
    double chunkDelayMaxS{0.0};
    /// Venue min order value in quote currency. 0 = take it from InstrumentSpec.
    double minNotional{0.0};
    /// Hard-reject exponential backoff ladder.
    double rejectBackoffBaseS{0.5};
    double rejectBackoffMaxS{30.0};
    int maxHardRejects{20};
    /// Headroom above the venue min-notional floor for the iceberg chunk clamp
    /// (venue checks against ITS mark/last, not our resting limit price).
    double minNotionalSafetyBuffer{1.10};
    /// A sub-min final slice within this fraction of the floor is rounded UP to
    /// clear it; below it the slice is skipped (rounding up would overshoot).
    double subMinRoundupThreshold{0.80};
    /// Derive the reduce-only (open vs close) decision from the CURRENT position
    /// and the live-mid order side, instead of taking the caller's reduceOnly
    /// verbatim. Needed on venues whose order side codes ARE the open/close
    /// semantic and do NOT auto-net (MEXC): a Sell that reduces a long must go
    /// out as a Close, not an Open, or it opens a second opposite position.
    /// Default false — Bybit/Lighter net on the venue and use reduceOnly as
    /// passed. When true, an order whose side opposes the held position is forced
    /// reduce-only (Close-side); a direction flip therefore closes the old side
    /// (benign 2009 on the excess) and the next cycle opens the new side flat.
    bool deriveReduceOnlyFromPosition{false};
};

/**
 * Venue-agnostic chasing-limit / iceberg executor — C++ port of the Python
 * LimitChasingExecutor from the Nautilus crypto-portfolio (common/live_executor.py).
 *
 * For one instrument, execute() chases the target net position with post-only
 * limit orders at the top of the book: submit at best bid/ask (+aggressiveness),
 * amend as the book moves (cancel+resubmit fallback), fill accounting driven by
 * the gateway's order/fill event stream, iceberg chunking with min-notional
 * clamps, classified reject handling with exponential backoff, and a hard
 * deadline (funding settlement minus buffer). Maker-only by construction —
 * the post-only flag guarantees the spread is never crossed.
 *
 * Threading: execute() blocks and is designed to run on a per-leg worker
 * thread; concurrent execute() calls on DISTINCT symbols are safe. Event
 * callbacks arrive on the gateway's io thread and only update per-leg state.
 *
 * Doctrine (carry strategies): an unfilled OPEN leg is skipped, an unfilled
 * CLOSE leg is retried next cycle — never market-out. The single exception is
 * the dust full-close (reduce-only market, venue min-notional exemption).
 */
class ChasingLimitExecutor {
    struct P;
    std::unique_ptr<P> m_p{};

public:
    /**
     * @param gateway venue adapter; must outlive the executor. The executor
     *        registers itself as the gateway's order/fill event sink.
     * @param cfg chase parameters
     * @param algoPrefix short strategy tag (e.g. "dc") stamped into every
     *        client order id so venue-side positions can be attributed on a
     *        shared (sub)account.
     */
    ChasingLimitExecutor(IExecutionGateway &gateway, const ChasingConfig &cfg, std::string algoPrefix);

    ~ChasingLimitExecutor();

    /**
     * Chase one instrument until the target net position is reached or the
     * deadline hits.
     * @param symbol venue symbol, e.g. XRPUSDT
     * @param targetSignedNotional strategy intent in quote currency: positive
     *        = target long, negative = target short, 0 = flat. Converted to a
     *        signed qty at the first fresh mid price observed in this call.
     * @param currentSignedQty current net position qty (signed) — the executor
     *        trades the delta.
     * @param deadline hard stop (wall clock). The funding-settlement contract:
     *        no fill may happen past it; a resting order is cancelled at exit.
     * @param reduceOnly every order carries reduce-only (cleanup phase safety:
     *        the venue rejects anything that would open/extend).
     * @param stopToken cooperative abort (app shutdown) — checked between
     *        chunks, never mid-request.
     */
    [[nodiscard]] ExecutionResult execute(const std::string &symbol, double targetSignedNotional, double currentSignedQty,
                                          std::chrono::system_clock::time_point deadline, bool reduceOnly, std::stop_token stopToken);
};

} // namespace stonky::execution

#endif // DIRTY_CARRY_CHASING_LIMIT_EXECUTOR_H
