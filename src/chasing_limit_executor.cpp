/**
Chasing Limit Executor

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2026 Vitezslav Kot <vitezslav.kot@stonky.cz>, Stonky s.r.o.

C++ port of the Python LimitChasingExecutor (crypto-portfolio/common/
live_executor.py). The order-management state machine, the min-notional
arithmetic and the reject-classification reactions are ported 1:1; the
delivery mechanics differ (per-leg worker thread + condition variable instead
of asyncio, synchronous gateway submit acks instead of Nautilus's async
submit pipeline).
*/

#include <stonky/common/chasing_limit_executor.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <map>
#include <mutex>
#include <random>
#include <set>
#include <thread>

namespace stonky::execution {
using SteadyClock = std::chrono::steady_clock;
using SteadyTime = SteadyClock::time_point;

namespace {
double snapDown(const double value, const double step) {
    if (step <= 0.0) {
        return value;
    }

    return std::floor(value / step + 1e-9) * step;
}

double snapUp(const double value, const double step) {
    if (step <= 0.0) {
        return value;
    }

    return std::ceil(value / step - 1e-9) * step;
}

/// An adapter-side quota gate deliberately returns Throttled without reaching
/// the venue. It must not pollute the final reject count/diagnostic, which are
/// intended to describe exchange behavior.
bool isLocalOrderPacing(const std::string &reason) {
    return reason.find("local order lane") != std::string::npos;
}

/// Per-leg running state for a single execute() call. The leg's worker thread
/// and the gateway's io thread (event callbacks) synchronize on `m`/`cv`.
struct LegState {
    std::mutex m;
    std::condition_variable cv;

    /// Everything below is guarded by m.
    std::string symbol;
    OrderSide side{OrderSide::Buy};
    double targetQty{}; /// snapped to qty step
    double filledQty{};
    double filledNotional{};
    std::string activeOrder{}; /// clientOrderId of the resting order; empty = none
    double activePrice{};
    /// A cancel was accepted by the venue and its terminal event is pending.
    /// Do not send the same cancel again for every quote tick in that gap.
    std::string cancelPendingOrder{};
    /// Earliest time the active order may be re-priced. This is deliberately
    /// separate from the regular wait interval: quote updates may still wake
    /// the leg for fills, but cannot turn into REST traffic before this gate.
    SteadyTime nextRepriceAt{};
    /// Last seen best price on OUR side of the book (bid for BUY, ask for
    /// SELL). Quote events wake the chase only when this side moves —
    /// opposite-side flicker doesn't change our target price.
    double lastRelevantPrice{};
    double maxChunkQty{}; /// effective iceberg chunk (qty); 0 = unchunked
    int numReposts{};
    int numCancels{};
    int numRejects{};
    int numHardRejects{}; /// non-benign only — drives backoff escalation + fatal cap
    std::string lastReject{}; /// most recent reject reason — surfaces in the exec report
    int numChunks{};
    std::string fatalError{};
    /// Set by a benign terminal reject (PositionClosed): the leg's goal is
    /// already met, so stop the chase WITHOUT recording a fatalError — the
    /// result reads as a skip (note set), never a failure.
    bool benignDone{false};
    std::string skipNote{};
    SteadyTime delayUntil{}; /// iceberg inter-chunk jitter gate
    SteadyTime backoffUntil{}; /// reject backoff gate
    std::set<std::string> seenFillIds; /// fill dedup (events may re-deliver)
    /// Per-order accounted fill qty. Together with `fillGaps` this closes the
    /// terminal-before-fill ordering race (audit 2026-08-12 #6): a Filled or
    /// Cancelled order update clears the active handle immediately, but qty
    /// lands via fill events — a worker that recomputed the remainder from a
    /// stale filledQty in that gap would submit a second child order, and the
    /// delayed fill would push the position past target.
    std::map<std::string, double> orderFilled;
    /// A terminal order update declared MORE cumulative fill than the fill
    /// events delivered so far: hold new submits behind a short barrier until
    /// the fills land, then (barrier expired) reconcile the difference from
    /// the venue's authoritative cumulative figure.
    struct FillGap {
        double cumQty{};
        double price{}; /// best price hint for a synthesized fill
        SteadyTime barrierUntil{};
    };
    std::map<std::string, FillGap> fillGaps;
    /// Orders whose TERMINAL event (Filled/Cancelled/Rejected) was already
    /// seen. A terminal event can beat the submit's REST ack (live-observed:
    /// a post-only cross lands mid-roundtrip) — the submit path consults this
    /// so it never marks a dead order as resting.
    std::set<std::string> terminalIds;
    /// Orders in UNKNOWN venue state: the submit's REST ack was lost to a
    /// transport failure, so the order may or may not be resting. While any
    /// orphan is unresolved the leg must not submit new orders (overfill
    /// risk); each loop iteration re-attempts a cancel until the venue
    /// confirms gone (cancel()==false) or a terminal event arrives.
    std::set<std::string> orphanIds;
    /// Orders still in unknown venue state when the teardown resolution wait
    /// exhausted — surfaced in ExecutionResult so the caller can alert; the
    /// venue may hold a live order the accounting no longer tracks.
    std::vector<std::string> unresolvedIds;
    bool wake{false};

    void notify() {
        wake = true;
        cv.notify_all();
    }
};
} // namespace

struct ChasingLimitExecutor::P {
    IExecutionGateway &gateway;
    ChasingConfig cfg;
    std::string algoPrefix;

    /// clientOrderId → leg routing for event callbacks. Entries persist until
    /// the leg's teardown sweep so trailing fills (delivered after a cancel
    /// ack) still land; fill dedup makes retained routes harmless.
    std::mutex routesM;
    std::map<std::string, std::shared_ptr<LegState>> orderRoutes;
    /// symbol → leg for quote-event wakeups. One execute() per symbol at a
    /// time (strategy doctrine); registered for the lifetime of the call.
    std::map<std::string, std::shared_ptr<LegState>> symbolRoutes;

    std::atomic<std::uint64_t> idCounter{0};
    std::mutex rngM;
    std::mt19937 rng{std::random_device{}()};

    P(IExecutionGateway &gw, const ChasingConfig &config, std::string prefix) : gateway(gw), cfg(config), algoPrefix(std::move(prefix)) {}

    // ── Small helpers ───────────────────────────────────────────────

    std::string makeClientOrderId() {
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        /// e.g. "dc-19c8f3a2b45-1a" — unique across restarts, ≤ 36 chars
        return fmt::format("{}-{:x}-{:x}", algoPrefix, ms, idCounter.fetch_add(1) + 1);
    }

    double uniformSample(const double lo, const double hi) {
        std::lock_guard lk(rngM);
        return std::uniform_real_distribution(lo, hi)(rng);
    }

    [[nodiscard]] std::shared_ptr<LegState> route(const std::string &clientOrderId) {
        std::lock_guard lk(routesM);

        if (const auto it = orderRoutes.find(clientOrderId); it != orderRoutes.end()) {
            return it->second;
        }

        return nullptr;
    }

    void addRoute(const std::string &clientOrderId, const std::shared_ptr<LegState> &leg) {
        std::lock_guard lk(routesM);
        orderRoutes[clientOrderId] = leg;
    }

    void removeRoute(const std::string &clientOrderId) {
        std::lock_guard lk(routesM);
        orderRoutes.erase(clientOrderId);
    }

    void sweepRoutes(const std::shared_ptr<LegState> &leg) {
        std::lock_guard lk(routesM);
        std::erase_if(orderRoutes, [&leg](const auto &kv) { return kv.second == leg; });
    }

    /// Registers the symbol → leg wakeup route. Returns false when a route for
    /// the symbol already exists — a second concurrent execute() on the same
    /// symbol would silently overwrite the first leg's route, and the older
    /// call's teardown would then unsubscribe the newer call's quotes.
    [[nodiscard]] bool addSymbolRoute(const std::string &symbol, const std::shared_ptr<LegState> &leg) {
        std::lock_guard lk(routesM);
        return symbolRoutes.try_emplace(symbol, leg).second;
    }

    void removeSymbolRoute(const std::string &symbol) {
        std::lock_guard lk(routesM);
        symbolRoutes.erase(symbol);
    }

    [[nodiscard]] std::shared_ptr<LegState> symbolRoute(const std::string &symbol) {
        std::lock_guard lk(routesM);

        if (const auto it = symbolRoutes.find(symbol); it != symbolRoutes.end()) {
            return it->second;
        }

        return nullptr;
    }

    [[nodiscard]] bool hasRoute(const std::shared_ptr<LegState> &leg) {
        std::lock_guard lk(routesM);
        return std::ranges::any_of(orderRoutes, [&leg](const auto &kv) { return kv.second == leg; });
    }

    // ── Event sinks (gateway io thread) ─────────────────────────────

    void onFill(const FillEvent &fill) {
        const auto leg = route(fill.clientOrderId);

        if (!leg) {
            return; /// not one of ours (another algo on the same account)
        }

        std::lock_guard lk(leg->m);

        /// Dedup re-delivered fills; distinct partial fills carry distinct ids.
        if (!fill.fillId.empty() && !leg->seenFillIds.insert(fill.fillId).second) {
            return;
        }

        double add = fill.qty;
        double &accounted = leg->orderFilled[fill.clientOrderId];

        /// If this order's terminal update declared a cumulative fill, the
        /// venue figure is authoritative: cap what fill events may add so a
        /// late fill arriving AFTER the gap was reconciled from that figure
        /// cannot double-count (audit #6).
        if (const auto it = leg->fillGaps.find(fill.clientOrderId); it != leg->fillGaps.end()) {
            add = std::min(add, std::max(0.0, it->second.cumQty - accounted));
        }

        accounted += add;
        if (add > 0.0) {
            leg->filledQty += add;
            leg->filledNotional += add * fill.price;
        }
        leg->notify();
    }

    /// Wake the chase the moment the relevant side of the book moves, rather
    /// than waiting out the repost interval (port of Python on_quote_tick).
    void onQuote(const std::string &symbol, const Quote &quote) {
        const auto leg = symbolRoute(symbol);

        if (!leg) {
            return; /// no chase running on this symbol
        }

        std::lock_guard lk(leg->m);
        const double relevant = leg->side == OrderSide::Buy ? quote.bid : quote.ask;

        if (relevant > 0.0 && relevant != leg->lastRelevantPrice) {
            leg->lastRelevantPrice = relevant;
            leg->notify();
        }
    }

    void onOrderUpdate(const OrderUpdate &update) {
        const auto leg = route(update.clientOrderId);

        if (!leg) {
            return;
        }

        std::lock_guard lk(leg->m);
        const bool isActive = leg->activeOrder == update.clientOrderId;
        const auto now = SteadyClock::now();

        if (update.state == OrderState::Filled || update.state == OrderState::Cancelled || update.state == OrderState::Rejected) {
            leg->terminalIds.insert(update.clientOrderId);
            leg->orphanIds.erase(update.clientOrderId);

            if (leg->cancelPendingOrder == update.clientOrderId) {
                leg->cancelPendingOrder.clear();
            }

            /// Terminal updates may carry (or PRECEDE) fills. When the venue's
            /// cumulative figure exceeds what fill events delivered so far,
            /// park the order behind a short barrier: the worker must not
            /// compute a remainder from the stale filledQty until the fills
            /// land or the barrier reconciles the gap (audit #6).
            if (update.cumFilledQty > 0.0) {
                if (const double accounted = leg->orderFilled[update.clientOrderId]; update.cumFilledQty > accounted + 1e-12) {
                    auto &gap = leg->fillGaps[update.clientOrderId];
                    gap.cumQty = std::max(gap.cumQty, update.cumFilledQty);
                    if (update.price > 0.0) {
                        gap.price = update.price;
                    } else if (gap.price <= 0.0) {
                        gap.price = leg->activePrice;
                    }
                    gap.barrierUntil = now + std::chrono::milliseconds(3000);
                }
            }
        }

        switch (update.state) {
            case OrderState::Accepted:
                /// Amend acks re-push the order with the venue-authoritative price.
                if (isActive && update.price > 0.0) {
                    leg->activePrice = update.price;
                }
                break;

            case OrderState::PartiallyFilled:
                break; /// qty accounting arrives via onFill

            case OrderState::Filled:
                if (isActive) {
                    leg->activeOrder.clear();

                    /// Iceberg time-jitter: randomized pause before the next chunk.
                    if (leg->maxChunkQty > 0.0 && cfg.chunkDelayMaxS > 0.0) {
                        leg->delayUntil = now + std::chrono::milliseconds(static_cast<std::int64_t>(uniformSample(cfg.chunkDelayMinS, cfg.chunkDelayMaxS) * 1000.0));
                    }
                }
                break;

            case OrderState::Cancelled:
                if (isActive) {
                    leg->activeOrder.clear();
                    leg->numCancels++;
                } else {
                    /// Venue-side cancel that raced ahead of the submit ack —
                    /// cause unknown (benign crosses arrive as Rejected), so
                    /// gate the resubmit briefly instead of looping tightly.
                    if (const auto defensive = now + std::chrono::milliseconds(500); defensive > leg->backoffUntil) {
                        leg->backoffUntil = defensive;
                    }
                }
                break;

            case OrderState::Rejected:
                if (isActive) {
                    leg->activeOrder.clear();
                }
                leg->numRejects++;
                applyRejectLocked(*leg, update.rejectKind, update.reason, now);
                break;
        }

        leg->notify();
    }

    /// Shared reaction to a classified reject — used by both async reject
    /// events and synchronous GatewayError throws. Caller holds leg.m.
    void applyRejectLocked(LegState &leg, const RejectKind kind, const std::string &reason, const SteadyTime now) const {
        const bool locallyPaced = kind == RejectKind::Throttled && isLocalOrderPacing(reason);

        if (!locallyPaced) {
            leg.lastReject = reason;
        }

        /// Benign crosses are routine chase noise (debug); everything else is
        /// operator-relevant — a reject storm must be visible in the log AS IT
        /// HAPPENS, not reconstructed from counters afterwards.
        if (kind == RejectKind::BenignPostOnlyCross) {
            spdlog::debug("{}: benign post-only cross ({})", leg.symbol, reason);
        } else if (kind == RejectKind::PositionClosed) {
            spdlog::info("{}: reduce skipped — venue reports the position already closed ({})", leg.symbol, reason);
        } else if (locallyPaced) {
            /// This was our shared MEXC pacing gate, not a venue-visible
            /// reject. Keep the live log focused on genuine exchange 510s.
            spdlog::debug("{}: order operation paced locally ({})", leg.symbol, reason);
        } else {
            const char *label = kind == RejectKind::MinNotional ? "MinNotional"
                                : kind == RejectKind::Permanent ? "Permanent"
                                : kind == RejectKind::Throttled ? "Throttled"
                                                                : "Hard";
            spdlog::warn("{}: order rejected [{}]: {}", leg.symbol, label, reason);
        }

        switch (kind) {
            case RejectKind::BenignPostOnlyCross: {
                /// Normal on a moving chase. Small floor so an illiquid sticky
                /// book doesn't draw a resubmit storm; NOT counted to the cap.
                if (const auto floor = now + std::chrono::milliseconds(250); floor > leg.backoffUntil) {
                    leg.backoffUntil = floor;
                }
                break;
            }

            case RejectKind::MinNotional:
                if (leg.filledQty > 0.0) {
                    /// Untradeable tail after the bulk filled — back off briefly,
                    /// the submit-side guard ends the chase as PARTIAL.
                    leg.backoffUntil = now + std::chrono::milliseconds(250);
                } else {
                    leg.fatalError = fmt::format("venue rejected for sub-minimum order value; reason={}", reason);
                }
                break;

            case RejectKind::Permanent:
                leg.fatalError = fmt::format("permanent reject, no retry (fix at venue/config); reason={}", reason);
                break;

            case RejectKind::Throttled:
                /// Venue congestion — the order never rested. Back off a full
                /// repost interval (well past the venue's per-second window) and
                /// retry within the deadline; NOT counted to the fatal cap, so a
                /// throttle storm can never abort a leg — it just fills late or
                /// times out honestly as UNFILLED.
                leg.backoffUntil = now + std::chrono::milliseconds(static_cast<std::int64_t>(std::max(cfg.repostIntervalS, 2.0) * 1000.0));
                break;

            case RejectKind::PositionClosed:
                /// The position we meant to reduce is already gone — goal met.
                /// End the leg cleanly as a skip (never a failure); a retry would
                /// only draw the same reject until the hard-reject cap.
                leg.benignDone = true;
                if (leg.skipNote.empty()) {
                    leg.skipNote = fmt::format("reduce no-op — venue reports position already closed ({})", reason);
                }
                break;

            case RejectKind::Hard: {
                leg.numHardRejects++;
                const double backoffS = std::min(cfg.rejectBackoffBaseS * std::pow(2.0, std::max(0, leg.numHardRejects - 1)), cfg.rejectBackoffMaxS);
                leg.backoffUntil = now + std::chrono::milliseconds(static_cast<std::int64_t>(backoffS * 1000.0));

                if (leg.numHardRejects >= cfg.maxHardRejects) {
                    leg.fatalError = fmt::format("too many hard rejects ({}); last reason={}", leg.numHardRejects, reason);
                }
                break;
            }
        }

        /// Wake the worker so it re-evaluates immediately instead of idling out a
        /// full repost interval in cv.wait_until — a synchronous submit reject
        /// (GatewayError catch) does not otherwise notify, unlike the async
        /// order-event path. Matters most for benignDone/fatal, which want to end
        /// the leg NOW; backoff kinds just re-check and sleep on backoffUntil.
        leg.notify();
    }

    // ── Quotes / pricing ────────────────────────────────────────────

    [[nodiscard]] std::optional<Quote> freshQuote(const std::string &symbol) const {
        auto quote = gateway.lastQuote(symbol);

        if (!quote || !quote->sane()) {
            return std::nullopt;
        }

        if (const auto ageS = std::chrono::duration_cast<std::chrono::milliseconds>(SteadyClock::now() - quote->receivedAt).count() / 1000.0; ageS > cfg.maxQuoteAgeS) {
            return std::nullopt;
        }

        return quote;
    }

    /// Maker price: BUY joins the bid queue, SELL joins the ask queue.
    /// aggressivenessTicks shifts INSIDE the spread, clamped so the result is
    /// always post-only-valid (BUY ≤ ask − tick, SELL ≥ bid + tick).
    [[nodiscard]] double makerPrice(const Quote &quote, const OrderSide side, const InstrumentSpec &spec) const {
        const double tick = spec.tickSize;
        const double aggression = cfg.aggressivenessTicks * tick;

        double raw;

        if (side == OrderSide::Buy) {
            raw = quote.bid + aggression;

            if (const double maxAllowed = quote.ask - tick; raw > maxAllowed) {
                raw = maxAllowed;
            }

            raw = snapDown(raw, tick);
        } else {
            raw = quote.ask - aggression;

            if (const double minAllowed = quote.bid + tick; raw < minAllowed) {
                raw = minAllowed;
            }

            raw = snapUp(raw, tick);
        }

        return raw > 0.0 ? raw : 0.0;
    }

    /// Wait for the first fresh, sane quote and return its mid. The notional →
    /// qty conversion sizes off this price; a stale cache entry from a prior
    /// cycle would mis-size the order, hence the freshness gate.
    [[nodiscard]] std::optional<double> waitForMid(const std::string &symbol, const double maxWaitS, const std::stop_token &stopToken) const {
        const auto until = SteadyClock::now() + std::chrono::milliseconds(static_cast<std::int64_t>(maxWaitS * 1000.0));

        while (SteadyClock::now() < until && !stopToken.stop_requested()) {
            if (const auto quote = freshQuote(symbol)) {
                return (quote->bid + quote->ask) / 2.0;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        return std::nullopt;
    }

    // ── Chunk sizing (iceberg + min-notional arithmetic) ────────────

    [[nodiscard]] double sampleChunkQty(const double maxChunkQty) {
        if (cfg.chunkQtyJitter <= 0.0) {
            return maxChunkQty;
        }

        const double floor = maxChunkQty * (1.0 - cfg.chunkQtyJitter);
        return uniformSample(floor, maxChunkQty);
    }

    /**
     * Resolve the next submit qty from the remaining target. Ports the Python
     * three-way min-notional reconciliation: (1) clamp a sub-floor chunk UP to
     * the buffered floor, (2) merge a sub-floor leftover into the current
     * chunk, (3) cap at the venue max order qty; plus the final sub-min guard
     * (roundup within threshold, otherwise skip with note). Caller holds leg.m.
     * Returns 0 when there is nothing tradeable; outSkipNote explains a
     * deliberate skip.
     */
    [[nodiscard]] double resolveSubmitQty(LegState &leg, const InstrumentSpec &spec, const double price, const double minNotional, std::string &outSkipNote) {
        const double remaining = leg.targetQty - leg.filledQty;

        if (remaining <= 0.0) {
            return 0.0;
        }

        double submitQty;

        if (leg.maxChunkQty > 0.0) {
            submitQty = std::min(remaining, sampleChunkQty(leg.maxChunkQty));

            if (minNotional > 0.0) {
                const double safeFloor = minNotional * cfg.minNotionalSafetyBuffer;
                const double minQtyClamp = snapUp(safeFloor / price, spec.qtyStep);

                /// (1) jitter/coarse-step pushed the chunk below the buffered floor
                if (submitQty * price < safeFloor) {
                    submitQty = std::min(remaining, minQtyClamp);
                }

                /// (2) absorb a sub-floor leftover into this chunk so the final
                /// iceberg tail always clears the venue min
                if (const double leftover = remaining - submitQty; leftover > 0.0 && leftover * price < safeFloor) {
                    submitQty = remaining;
                }
            }
        } else {
            submitQty = remaining;
        }

        /// (3) venue max order qty cap — overflow spills into the next cycle
        if (spec.maxQty > 0.0 && submitQty > spec.maxQty) {
            submitQty = spec.maxQty;
        }

        submitQty = snapDown(submitQty, spec.qtyStep);

        /// Chunk jitter can sample BELOW one qty step whenever a single lot is
        /// worth nearly the whole chunk notional (maxChunkQty == one step, i.e.
        /// lot value in [chunk·(1−jitter), chunk]) — snapped to 0 it would end
        /// the leg although whole lots remain (live-observed: 1000RATS_USDT
        /// "remaining 400 snaps to 0 (step 100)", UNFILLED every cycle; with
        /// MinNotional=0 the floor clamps above are inactive). Floor the slice
        /// at one step; only a genuinely sub-step REMAINDER is an untradeable
        /// tail.
        if (submitQty <= 0.0) {
            submitQty = std::min(spec.qtyStep, snapDown(remaining, spec.qtyStep));
        }

        if (submitQty <= 0.0) {
            outSkipNote = fmt::format("sub-lot tail: remaining {:.6g} snaps to 0 (step {:.6g})", remaining, spec.qtyStep);
            return 0.0;
        }

        /// Final sub-min guard (iceberg tail AND the unchunked single-order
        /// path): round a borderline slice UP to clear the floor, skip a
        /// genuinely-too-small one instead of drawing a fatal venue reject.
        if (minNotional > 0.0) {
            if (const double sliceNotional = submitQty * price; sliceNotional < minNotional) {
                if (sliceNotional >= minNotional * cfg.subMinRoundupThreshold) {
                    const double safeFloor = minNotional * cfg.minNotionalSafetyBuffer;
                    submitQty = snapUp(safeFloor / price, spec.qtyStep);
                    spdlog::info("{}: sub-min ${:.2f} within {:.0f}% of floor — rounding up to {}", leg.symbol, sliceNotional, cfg.subMinRoundupThreshold * 100.0, submitQty);
                } else {
                    outSkipNote = fmt::format("sub-min: ${:.2f} < ${:.2f} venue min (too small to round up)", sliceNotional, minNotional);
                    return 0.0;
                }
            }
        }

        return submitQty;
    }

    /// Cancel the resting order. When the venue reports it already gone
    /// (cancel() == false), the terminal event either raced ahead of us or is
    /// lost — clear the handle locally with a small defensive backoff so the
    /// loop cannot spin on amend/cancel of a ghost order (mirrors the Python
    /// cache-sync defensive path).
    void cancelActive(const std::shared_ptr<LegState> &leg, const std::string &clientOrderId) {
        {
            std::lock_guard lk(leg->m);

            /// The quote callback may wake us many times before MEXC delivers
            /// the Cancelled event. One accepted cancel is sufficient; repeats
            /// waste the private-order quota and can themselves draw a 513.
            if (leg->activeOrder != clientOrderId || leg->cancelPendingOrder == clientOrderId) {
                return;
            }

            leg->cancelPendingOrder = clientOrderId;
        }

        bool accepted = true;

        try {
            accepted = gateway.cancel(clientOrderId, leg->symbol);
        } catch (GatewayError &e) {
            std::lock_guard lk(leg->m);

            if (leg->cancelPendingOrder == clientOrderId) {
                leg->cancelPendingOrder.clear();
            }

            if (e.kind == RejectKind::Throttled) {
                const auto throttleBackoff = SteadyClock::now() + std::chrono::milliseconds(static_cast<std::int64_t>(std::max(cfg.repostIntervalS, 2.0) * 1000.0));
                leg->backoffUntil = std::max(leg->backoffUntil, throttleBackoff);
            }

            spdlog::debug("{}: cancel {} failed: {}", leg->symbol, clientOrderId, e.what());
            return;
        } catch (std::exception &e) {
            std::lock_guard lk(leg->m);

            if (leg->cancelPendingOrder == clientOrderId) {
                leg->cancelPendingOrder.clear();
            }

            spdlog::debug("{}: cancel {} failed: {}", leg->symbol, clientOrderId, e.what());
            return;
        }

        if (!accepted) {
            std::lock_guard lk(leg->m);

            if (leg->activeOrder == clientOrderId) {
                leg->activeOrder.clear();
                leg->cancelPendingOrder.clear();

                if (const auto defensive = SteadyClock::now() + std::chrono::milliseconds(500); defensive > leg->backoffUntil) {
                    leg->backoffUntil = defensive;
                }
            }
        }
    }

    // ── Core chase loop (leg worker thread) ─────────────────────────

    void chaseLoop(const std::shared_ptr<LegState> &leg, const InstrumentSpec &spec, const double minNotional, const SteadyTime steadyDeadline, const bool reduceOnly,
                   const std::stop_token &stopToken) {
        const double targetFill = leg->targetQty * cfg.fillTolerance;

        while (true) {
            const auto now = SteadyClock::now();

            if (now >= steadyDeadline || stopToken.stop_requested()) {
                return;
            }

            {
                std::lock_guard lk(leg->m);

                if (leg->filledQty >= targetFill || !leg->fatalError.empty() || leg->benignDone) {
                    return;
                }
            }

            const auto quote = freshQuote(leg->symbol);

            if (!quote) {
                /// Stale/dead feed with an order still resting: the order sits
                /// at a price computed from a book we can no longer see —
                /// adverse-selection bait until the deadline. Cancel it and
                /// stay out; a fresh submit requires a fresh quote by
                /// construction, so the chase resumes when the feed recovers.
                /// (cancelPendingOrder makes the repeat calls no-ops while the
                /// cancel is in flight.)
                std::string active;
                {
                    std::lock_guard lk(leg->m);
                    active = leg->activeOrder;
                }

                if (!active.empty()) {
                    cancelActive(leg, active);
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            const double targetPrice = makerPrice(*quote, leg->side, spec);

            if (targetPrice <= 0.0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            /// Snapshot the resting order; clear the wake flag BEFORE acting so
            /// any event fired by the actions below wakes the wait at the end.
            std::string activeOrder;
            double activePrice;
            std::string cancelPendingOrder;
            SteadyTime nextRepriceAt;
            {
                std::lock_guard lk(leg->m);
                activeOrder = leg->activeOrder;
                activePrice = leg->activePrice;
                cancelPendingOrder = leg->cancelPendingOrder;
                nextRepriceAt = leg->nextRepriceAt;
                leg->wake = false;
            }

            /// Re-price a resting order that is off the current target price.
            /// Half-tick tolerance, not float !=: activePrice is re-pushed from
            /// venue acks (a clean decimal echo) while targetPrice is a raw
            /// snapDown result — bit-inequality between the two representations
            /// of the SAME price level would cancel+resubmit at an identical
            /// price on every wake.
            const bool priceOff = spec.tickSize > 0.0 ? std::abs(activePrice - targetPrice) > spec.tickSize * 0.5 : activePrice != targetPrice;

            /// Advance-only mode: a target on the PASSIVE side of the resting
            /// price is a retreat — skip it and keep the front-of-queue spot;
            /// the market coming back fills us first. (leg->side is fixed for
            /// the whole chase, safe to read unlocked.)
            const bool retreats = cfg.repriceOnlyTowardMarket && priceOff &&
                                  (leg->side == OrderSide::Buy ? targetPrice < activePrice : targetPrice > activePrice);

            if (!activeOrder.empty() && priceOff && !retreats && cancelPendingOrder.empty() && now >= nextRepriceAt) {
                if (gateway.supportsAmend()) {
                    try {
                        gateway.amendPrice(activeOrder, leg->symbol, targetPrice);
                        std::lock_guard lk(leg->m);

                        /// Apply only if still the active order (it may have
                        /// filled or cancelled while the amend was in flight).
                        if (leg->activeOrder == activeOrder) {
                            leg->activePrice = targetPrice;
                            leg->numReposts++;

                            if (cfg.minRepriceIntervalS > 0.0) {
                                leg->nextRepriceAt = SteadyClock::now() + std::chrono::milliseconds(static_cast<std::int64_t>(cfg.minRepriceIntervalS * 1000.0));
                            }
                        }
                    } catch (std::exception &) {
                        /// Amend failed (order gone / new price would cross) —
                        /// cancel; the next iteration submits fresh.
                        cancelActive(leg, activeOrder);
                    }
                } else {
                    /// Venue without amend (e.g. MEXC): cancel + fresh submit.
                    cancelActive(leg, activeOrder);
                }
            }

            /// Orphan resolution: while any order is in unknown venue state,
            /// keep re-attempting its cancel and DO NOT submit (a phantom fill
            /// plus a fresh full-remaining order would overfill the target).
            bool orphansPending = false;
            {
                std::vector<std::string> orphans;
                {
                    std::lock_guard lk(leg->m);
                    orphans.assign(leg->orphanIds.begin(), leg->orphanIds.end());
                }

                for (const auto &orphanId: orphans) {
                    try {
                        if (!gateway.cancel(orphanId, leg->symbol)) {
                            std::lock_guard lk(leg->m);
                            leg->orphanIds.erase(orphanId); /// venue confirms gone
                        }
                        /// cancel accepted → the Cancelled event erases the orphan
                    } catch (std::exception &e) {
                        spdlog::debug("{}: orphan cancel retry failed: {}", leg->symbol, e.what());
                    }
                }

                std::lock_guard lk(leg->m);
                orphansPending = !leg->orphanIds.empty();
            }

            /// Submit a fresh order when nothing is resting — unless a gate holds.
            {
                std::unique_lock lk(leg->m);

                if (leg->activeOrder.empty() && !orphansPending) {
                    /// Fill-accounting barrier (audit #6): a terminal order
                    /// with fills its events have not delivered yet parks the
                    /// submit path. Within the barrier: wait for the fills.
                    /// Past it: book the difference from the venue's
                    /// cumulative figure, then re-evaluate from the loop top
                    /// (the reconciled qty may complete the leg).
                    {
                        const auto nowGap = SteadyClock::now();
                        const double eps = std::max(spec.qtyStep * 0.5, 1e-12);
                        SteadyTime gapBarrier{};
                        double reconciled = 0.0;
                        for (auto &[id, gap]: leg->fillGaps) {
                            const double room = gap.cumQty - leg->orderFilled[id];
                            if (room <= eps) {
                                continue;
                            }
                            if (nowGap < gap.barrierUntil) {
                                gapBarrier = std::max(gapBarrier, gap.barrierUntil);
                            } else {
                                const double px = gap.price > 0.0 ? gap.price : targetPrice;
                                leg->orderFilled[id] = gap.cumQty;
                                leg->filledQty += room;
                                leg->filledNotional += room * px;
                                reconciled += room;
                                spdlog::warn("{}: order {} fills reconciled from venue cumulative (+{:.6g} @ {:.6g}) — fill events late or lost", leg->symbol, id,
                                             room, px);
                            }
                        }
                        if (gapBarrier != SteadyTime{}) {
                            const auto sleepUntil = std::min(gapBarrier, steadyDeadline);
                            lk.unlock();
                            while (SteadyClock::now() < sleepUntil && !stopToken.stop_requested()) {
                                std::this_thread::sleep_until(std::min(sleepUntil, SteadyClock::now() + std::chrono::milliseconds(50)));
                            }
                            continue;
                        }
                        if (reconciled > 0.0) {
                            continue; /// target may be reached now — loop top decides
                        }
                    }

                    if (const auto gateUntil = std::max(leg->delayUntil, leg->backoffUntil); gateUntil > now) {
                        const auto sleepUntil = std::min(gateUntil, steadyDeadline);
                        lk.unlock();
                        /// Sliced so a stop request interrupts the wait — a full
                        /// reject-backoff (up to 30 s) must not stall shutdown.
                        while (SteadyClock::now() < sleepUntil && !stopToken.stop_requested()) {
                            std::this_thread::sleep_until(std::min(sleepUntil, SteadyClock::now() + std::chrono::milliseconds(200)));
                        }
                        continue;
                    }

                    std::string skipNote;
                    const double submitQty = resolveSubmitQty(*leg, spec, targetPrice, minNotional, skipNote);

                    if (submitQty <= 0.0) {
                        if (!skipNote.empty()) {
                            leg->skipNote = skipNote;
                        }

                        return; /// untradeable tail or target reached — end cleanly
                    }

                    const auto clientOrderId = makeClientOrderId();
                    lk.unlock();

                    /// Route BEFORE submit so a fast fill event cannot miss the leg.
                    addRoute(clientOrderId, leg);

                    /// Re-check the deadline at the last instant: the cancel and
                    /// orphan phases above can queue for seconds behind other
                    /// legs' serialized gateway ops, and the loop-top check is
                    /// long stale by then. A NEW child order past the deadline
                    /// would rest through the funding payout — never submit it.
                    if (stopToken.stop_requested() || SteadyClock::now() >= steadyDeadline) {
                        removeRoute(clientOrderId);
                        return;
                    }

                    try {
                        gateway.submitPostOnlyLimit(clientOrderId, leg->symbol, leg->side, submitQty, targetPrice, reduceOnly);

                        std::lock_guard lk2(leg->m);
                        leg->numReposts++;

                        if (leg->maxChunkQty > 0.0) {
                            leg->numChunks++;
                            leg->delayUntil = {}; /// fresh delay sampled when this chunk fills
                        }

                        /// The order's terminal event may have arrived DURING the
                        /// REST roundtrip (live-observed: an instant post-only
                        /// cross) — its reject reaction is already applied; do
                        /// not resurrect a dead order as resting.
                        if (!leg->terminalIds.contains(clientOrderId)) {
                            leg->activeOrder = clientOrderId;
                            leg->activePrice = targetPrice;

                            if (cfg.minRepriceIntervalS > 0.0) {
                                leg->nextRepriceAt = SteadyClock::now() + std::chrono::milliseconds(static_cast<std::int64_t>(cfg.minRepriceIntervalS * 1000.0));
                            }
                        }
                    } catch (GatewayError &e) {
                        /// Venue definitively rejected the submit — no order exists.
                        removeRoute(clientOrderId);
                        std::lock_guard lk2(leg->m);

                        if (!(e.kind == RejectKind::Throttled && isLocalOrderPacing(e.what()))) {
                            leg->numRejects++;
                        }

                        applyRejectLocked(*leg, e.kind, e.what(), SteadyClock::now());
                    } catch (std::exception &e) {
                        /// Transport-level failure — the venue MAY have accepted the
                        /// order without us seeing the ack. Safety-cancel; until the
                        /// venue confirms the order gone, it is an ORPHAN: the route
                        /// stays (events still land) and the loop stops submitting.
                        spdlog::warn("{}: submit transport failure ({}) — issuing safety cancel {}", leg->symbol, e.what(), clientOrderId);

                        bool confirmedGone = false;
                        bool cancelAccepted = false;

                        try {
                            cancelAccepted = gateway.cancel(clientOrderId, leg->symbol);
                            confirmedGone = !cancelAccepted;
                        } catch (std::exception &cancelErr) {
                            spdlog::error("{}: safety cancel failed — possible stray resting order {} ({})", leg->symbol, clientOrderId, cancelErr.what());
                        }

                        std::lock_guard lk2(leg->m);
                        leg->numRejects++;

                        if (confirmedGone) {
                            /// Never accepted (or already terminal) — no order exists.
                        } else if (!leg->terminalIds.contains(clientOrderId)) {
                            /// Cancel accepted (terminal event on its way) or cancel
                            /// itself failed — either way, resolution pending.
                            leg->orphanIds.insert(clientOrderId);
                        }

                        applyRejectLocked(*leg, RejectKind::Hard, e.what(), SteadyClock::now());
                    }
                }
            }

            /// Wait for an order/fill event or the repost interval, whichever
            /// comes first. Early wake = zero idle when fills arrive fast.
            {
                std::unique_lock lk(leg->m);
                const auto waitUntil = std::min(SteadyClock::now() + std::chrono::milliseconds(static_cast<std::int64_t>(cfg.repostIntervalS * 1000.0)), steadyDeadline);
                leg->cv.wait_until(lk, waitUntil, [&leg] { return leg->wake; });
                leg->wake = false;
            }
        }
    }

    /// Deadline/stop teardown: cancel the straggler, wait (bounded) for its
    /// resolution so the result doesn't under-report a fill landing in the
    /// cancel-ack gap, then sweep the event routes.
    /// Teardown cancel: unlike a mid-chase cancel (where the loop simply
    /// retries next iteration), a refused cancel here leaves a resting order
    /// behind for good — through the funding settlement the deadline was
    /// protecting. Local order-lane pacing and venue throttles are transient
    /// by definition (live-observed: the shared lane refusing a straggler
    /// cancel right after a burst of leg teardowns): retry briefly instead of
    /// giving up on the first refusal.
    [[nodiscard]] bool cancelAtTeardown(const std::shared_ptr<LegState> &leg, const std::string &clientOrderId) const {
        for (int attempt = 0;; ++attempt) {
            try {
                return gateway.cancel(clientOrderId, leg->symbol);
            } catch (GatewayError &e) {
                if (e.kind != RejectKind::Throttled || attempt >= 5) {
                    throw;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(1500));
            }
        }
    }

    void finishLeg(const std::shared_ptr<LegState> &leg) {
        std::string active;
        {
            std::lock_guard lk(leg->m);

            /// The last chunk's fill (execution event) can end the chase before
            /// its Filled ORDER event lands — don't waste a REST cancel on an
            /// order already known terminal; just drop the handle.
            if (!leg->activeOrder.empty() && leg->terminalIds.contains(leg->activeOrder)) {
                leg->activeOrder.clear();
            }

            active = leg->activeOrder;
        }

        std::vector<std::string> orphans;
        {
            std::lock_guard lk(leg->m);
            orphans.assign(leg->orphanIds.begin(), leg->orphanIds.end());
        }

        for (const auto &orphanId: orphans) {
            try {
                if (!cancelAtTeardown(leg, orphanId)) {
                    std::lock_guard lk(leg->m);
                    leg->orphanIds.erase(orphanId);
                }
            } catch (std::exception &e) {
                spdlog::error(fmt::format("{}: orphan cancel failed at teardown — verify no stray order {} on the venue ({})", leg->symbol, orphanId, e.what()));
            }
        }

        if (!active.empty()) {
            try {
                if (!cancelAtTeardown(leg, active)) {
                    /// Already gone at the venue — no Cancelled event will come;
                    /// clear the handle so the resolution wait below is a no-op
                    /// (fills were accounted via the execution stream).
                    std::lock_guard lk(leg->m);

                    if (leg->activeOrder == active) {
                        leg->activeOrder.clear();
                    }
                }
            } catch (std::exception &e) {
                spdlog::warn(fmt::format("{}: straggler cancel failed: {}", leg->symbol, e.what()));
            }
        }

        if (!active.empty() || !orphans.empty() || hasRoute(leg)) {
            /// Resolution wait: ≤3 s for the cancel/fill acks to clear the
            /// active handle AND all orphans.
            for (int i = 0; i < 30; ++i) {
                {
                    std::lock_guard lk(leg->m);

                    if (leg->activeOrder.empty() && leg->orphanIds.empty()) {
                        break;
                    }
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

            {
                std::lock_guard lk(leg->m);

                if (!leg->activeOrder.empty() || !leg->orphanIds.empty()) {
                    /// Record the ids for the ExecutionResult — a stray order
                    /// the venue may still hold is an operator alert, not just
                    /// a log line.
                    if (!leg->activeOrder.empty()) {
                        leg->unresolvedIds.push_back(leg->activeOrder);
                    }

                    leg->unresolvedIds.insert(leg->unresolvedIds.end(), leg->orphanIds.begin(), leg->orphanIds.end());

                    spdlog::warn(fmt::format("{}: order resolution wait exhausted (3 s, orphans={}) — result may under-report fills; verify venue for stray orders",
                                             leg->symbol, leg->orphanIds.size()));
                }
            }

            /// Grace beat: a trailing fill report can arrive after the cancel
            /// ack (no cross-topic ordering guarantee); routes stay alive for it.
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }

        sweepRoutes(leg);

        /// Post-sweep reconciliation (audit #6): no more events can land now,
        /// so any remaining terminal-declared fills the events never delivered
        /// are booked from the venue's cumulative figure — the result must not
        /// under-report qty the account actually holds.
        {
            std::lock_guard lk(leg->m);
            for (auto &[id, gap]: leg->fillGaps) {
                const double room = gap.cumQty - leg->orderFilled[id];
                if (room <= 1e-12) {
                    continue;
                }
                const double px = gap.price > 0.0 ? gap.price : (leg->filledQty > 0.0 ? leg->filledNotional / leg->filledQty : 0.0);
                leg->orderFilled[id] = gap.cumQty;
                leg->filledQty += room;
                leg->filledNotional += room * px;
                spdlog::warn("{}: order {} fills reconciled at teardown (+{:.6g} @ {:.6g}) — fill events never arrived", leg->symbol, id, room, px);
            }
        }
    }

    /// Dust full-close: ONE reduce-only market order (venue min-notional
    /// exemption). A maker chase cannot buy anything on a sub-min notional.
    void marketCloseDust(const std::shared_ptr<LegState> &leg, const double currentSignedQty, const double dustQty) {
        const OrderSide side = currentSignedQty < 0.0 ? OrderSide::Buy : OrderSide::Sell;

        {
            std::lock_guard lk(leg->m);
            leg->side = side;
            leg->targetQty = dustQty;
        }

        const auto clientOrderId = makeClientOrderId();
        addRoute(clientOrderId, leg);

        try {
            gateway.submitReduceOnlyMarket(clientOrderId, leg->symbol, side, dustQty);
            spdlog::info(fmt::format("{}: dust close — reduce-only market {} {} (sub-min exemption)", leg->symbol, side == OrderSide::Buy ? "Buy" : "Sell", dustQty));
        } catch (std::exception &e) {
            sweepRoutes(leg);
            std::lock_guard lk(leg->m);
            leg->fatalError = fmt::format("dust close submit failed: {}", e.what());
            return;
        }

        /// Market orders fill ~instantly; 5 s covers ack + fill propagation.
        for (int i = 0; i < 50; ++i) {
            {
                std::lock_guard lk(leg->m);

                if (leg->filledQty >= dustQty || !leg->fatalError.empty()) {
                    break;
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        {
            std::lock_guard lk(leg->m);

            if (leg->filledQty < dustQty && leg->fatalError.empty()) {
                leg->skipNote = fmt::format("dust close submitted ({}) but fill not confirmed in 5 s — verify on venue / next sync", dustQty);
            }
        }

        sweepRoutes(leg);
    }
};

ChasingLimitExecutor::ChasingLimitExecutor(IExecutionGateway &gateway, const ChasingConfig &cfg, std::string algoPrefix) :
    m_p(std::make_unique<P>(gateway, cfg, std::move(algoPrefix))) {
    gateway.setFillCallback([this](const FillEvent &fill) { m_p->onFill(fill); });
    gateway.setOrderUpdateCallback([this](const OrderUpdate &update) { m_p->onOrderUpdate(update); });
    gateway.setQuoteCallback([this](const std::string &symbol, const Quote &quote) { m_p->onQuote(symbol, quote); });
}

ChasingLimitExecutor::~ChasingLimitExecutor() = default;

ExecutionResult ChasingLimitExecutor::execute(const std::string &symbol, const double targetSignedNotional, const double currentSignedQty,
                                              const std::chrono::system_clock::time_point deadline, const bool reduceOnly, std::stop_token stopToken) {
    const auto startTime = SteadyClock::now();
    /// Wall-clock deadline → steady-clock (immune to NTP steps mid-chase).
    const auto steadyDeadline = startTime + std::chrono::duration_cast<SteadyClock::duration>(deadline - std::chrono::system_clock::now());

    const auto leg = std::make_shared<LegState>();
    leg->symbol = symbol;

    const auto buildResult = [&](const std::optional<OrderSide> side, const double targetQtyRaw) {
        std::lock_guard lk(leg->m);
        ExecutionResult result;
        result.symbol = symbol;
        result.side = side;
        result.targetQty = leg->targetQty > 0.0 ? leg->targetQty : targetQtyRaw;
        result.filledQty = leg->filledQty;
        result.avgFillPrice = leg->filledQty > 0.0 ? leg->filledNotional / leg->filledQty : 0.0;
        result.numReposts = leg->numReposts;
        result.numCancels = leg->numCancels;
        result.numRejects = leg->numRejects;
        result.numChunksSubmitted = leg->numChunks;
        result.elapsedS = std::chrono::duration_cast<std::chrono::milliseconds>(SteadyClock::now() - startTime).count() / 1000.0;
        result.error = leg->fatalError;
        result.note = leg->skipNote;
        result.lastReject = leg->lastReject;
        result.unresolvedOrders = leg->unresolvedIds;

        if (!leg->fatalError.empty()) {
            result.status = ExecStatus::ABORTED;
        } else if (leg->targetQty > 0.0 && leg->filledQty >= leg->targetQty * m_p->cfg.fillTolerance) {
            result.status = ExecStatus::FILLED;
        } else if (leg->filledQty > 0.0) {
            result.status = ExecStatus::PARTIAL;
        } else {
            result.status = ExecStatus::UNFILLED;
        }

        return result;
    };

    /// Quote wakeups route by symbol; the guard tears the route and the
    /// ticker subscription down on EVERY exit path (Python's finally-block
    /// unsubscribe — subscriptions must not accumulate over a month-long run).
    /// A refused registration = another execute() is still chasing this symbol
    /// (one-execute-per-symbol doctrine) — abort THIS call instead of hijacking
    /// the running leg's route and subscription.
    if (!m_p->addSymbolRoute(symbol, leg)) {
        spdlog::error("{}: concurrent execute() refused — another chase for this symbol is still running", symbol);

        {
            std::lock_guard lk(leg->m);
            leg->fatalError = "concurrent execute() on the same symbol refused";
        }

        return buildResult(std::nullopt, 0.0);
    }

    struct QuoteCleanup {
        P *p;
        std::string symbol;

        ~QuoteCleanup() {
            p->removeSymbolRoute(symbol);

            try {
                p->gateway.unsubscribeQuotes(symbol);
            } catch (std::exception &e) {
                spdlog::debug("{}: quote unsubscribe failed: {}", symbol, e.what());
            }
        }
    } quoteCleanup{m_p.get(), symbol};

    InstrumentSpec spec;

    try {
        spec = m_p->gateway.instrumentSpec(symbol);
        m_p->gateway.subscribeQuotes(symbol);
    } catch (std::exception &e) {
        {
            std::lock_guard lk(leg->m);
            leg->fatalError = fmt::format("instrument/quote setup failed: {}", e.what());
        }
        return buildResult(std::nullopt, 0.0);
    }

    const double minNotional = m_p->cfg.minNotional > 0.0 ? m_p->cfg.minNotional : spec.minNotional;

    /// First fresh quote → mid → notional-to-qty conversion.
    const auto mid = m_p->waitForMid(symbol, 5.0, stopToken);

    if (!mid || *mid <= 0.0) {
        {
            std::lock_guard lk(leg->m);
            leg->fatalError = "no valid quote within 5 s — cannot derive target qty from notional";
        }
        return buildResult(std::nullopt, 0.0);
    }

    const double targetSignedQty = targetSignedNotional / *mid;
    const double deltaSignedQty = targetSignedQty - currentSignedQty;
    const double deltaAbsNotional = std::abs(deltaSignedQty) * *mid;

    /// Sub-min skip — a benign filter, not a failure. Exception: dust
    /// full-close goes out as ONE reduce-only market order (venue exempts
    /// reduce-only closes from the min-notional check).
    if (minNotional > 0.0 && deltaAbsNotional < minNotional) {
        if (reduceOnly && targetSignedNotional == 0.0) {
            if (const double dustQty = snapDown(std::abs(currentSignedQty), spec.qtyStep); dustQty > 0.0 && dustQty >= spec.minQty) {
                m_p->marketCloseDust(leg, currentSignedQty, dustQty);
                OrderSide dustSide;
                {
                    std::lock_guard lk(leg->m);
                    dustSide = leg->side;
                }
                return buildResult(dustSide, dustQty);
            }
        }

        {
            std::lock_guard lk(leg->m);
            leg->skipNote = fmt::format("sub-min: delta ${:.2f} < ${:.2f} venue min notional", deltaAbsNotional, minNotional);
        }

        return buildResult(std::nullopt, 0.0);
    }

    if (deltaSignedQty == 0.0) {
        return buildResult(std::nullopt, 0.0); /// already at target
    }

    const OrderSide side = deltaSignedQty > 0.0 ? OrderSide::Buy : OrderSide::Sell;
    const double targetQtyRaw = std::abs(deltaSignedQty);
    bool subLot = false;

    /// On a venue whose side codes ARE the open/close semantic and do NOT auto-net
    /// (MEXC), force reduce-only whenever this order's side OPPOSES the held
    /// position — a Sell reducing a long, or a Buy reducing a short — so it goes
    /// out Close-side instead of opening a second opposite position (dust). Decided
    /// from the SAME live-mid delta the side came from, so side and open/close can
    /// never disagree. This handles same-side magnitude REDUCTIONS (a trim); a
    /// direction flip is pre-resolved by the MEXC executor into an explicit
    /// target-0 reduce-only close (opening the new side next cycle), so it never
    /// arrives here as a straddle-zero order.
    /// A reduce-only order must OPPOSE the held position. The cleanup sizes its
    /// trims from ENTRY-price notionals while the qty delta above is priced at
    /// the LIVE mid — with enough drift between the two, a long "excess" trim
    /// resolves to a BUY (and vice versa), which the venue can only read as a
    /// close of the side we do not hold (live-observed hourly: SKL/PARTI :01
    /// cleanup drawing 2009 "position already closed" with the position alive).
    /// At/below target at live prices means there is nothing to trim — skip
    /// cleanly, never submit.
    if (reduceOnly && currentSignedQty != 0.0 && (side == OrderSide::Buy) == (currentSignedQty > 0.0)) {
        {
            std::lock_guard lk(leg->m);
            leg->skipNote = fmt::format("reduce skip: {} trim resolves to {} at the live mid — at/below target, nothing to reduce (entry-vs-mark drift)",
                                        currentSignedQty > 0.0 ? "long" : "short", side == OrderSide::Buy ? "Buy" : "Sell");
        }
        return buildResult(side, 0.0);
    }

    bool effectiveReduceOnly = reduceOnly;
    if (m_p->cfg.deriveReduceOnlyFromPosition && !reduceOnly && currentSignedQty != 0.0) {
        effectiveReduceOnly = (side == OrderSide::Buy) == (currentSignedQty < 0.0);
    }

    {
        std::lock_guard lk(leg->m);
        leg->side = side;
        leg->targetQty = snapDown(targetQtyRaw, spec.qtyStep);

        /// Pre-flight venue-min check: raw qty below the size step, or snapped
        /// qty below the instrument minimum — a strategy-intent/venue-rules
        /// mismatch, skipped cleanly (not a runtime failure).
        if (leg->targetQty <= 0.0 || leg->targetQty < spec.minQty) {
            leg->targetQty = 0.0;
            leg->skipNote = fmt::format("sub-lot: delta ${:.2f} = qty {:.6g} < lot {:.6g}/min {:.6g}", deltaAbsNotional, targetQtyRaw, spec.qtyStep, spec.minQty);
            subLot = true;
        }

        /// Effective iceberg chunk for this instrument at the live mid. Snapped
        /// DOWN to the step; when one step is worth more than the chunk
        /// notional it snaps to 0 → treat as unchunked.
        if (m_p->cfg.maxChunkNotional > 0.0) {
            leg->maxChunkQty = snapDown(m_p->cfg.maxChunkNotional / *mid, spec.qtyStep);
        }
    }

    if (subLot) {
        return buildResult(side, targetQtyRaw);
    }

    m_p->chaseLoop(leg, spec, minNotional, steadyDeadline, effectiveReduceOnly, stopToken);
    m_p->finishLeg(leg);

    return buildResult(side, targetQtyRaw);
}

} // namespace stonky::execution
