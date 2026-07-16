/**
Execution Gateway Interface

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2026 Vitezslav Kot <vitezslav.kot@stonky.cz>, Stonky s.r.o.
*/

#ifndef I_EXECUTION_GATEWAY_H
#define I_EXECUTION_GATEWAY_H

#include <chrono>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>

namespace stonky::execution {

/**
 * Venue-agnostic execution layer. ChasingLimitExecutor (the iceberg/chase core)
 * talks ONLY to this interface; one adapter per venue (Bybit first, then MEXC,
 * Lighter) translates it to the venue's REST/WS API. Nothing venue-specific may
 * leak through here — venue reject strings, symbol conventions and endpoint
 * quirks belong inside the adapters.
 */

enum class OrderSide { Buy, Sell };

/// Venue reject classification. Adapters map venue-specific codes/strings to
/// these classes; the chase core decides the reaction:
///  - BenignPostOnlyCross: our maker limit would have crossed — normal on a
///    moving book, repost at the new top (short backoff, never fatal).
///  - MinNotional: order below the venue's min order value — fatal when nothing
///    filled yet, benign tail-end when the bulk already filled.
///  - Permanent: no retry can succeed this cycle (delisting, API key/IP config,
///    unsigned agreement) — abort the leg immediately.
///  - Throttled: the venue rate-limited us (e.g. MEXC 510 "requests too
///    frequent") — the order never reached the book, so back off HARD and retry
///    WITHOUT counting toward the fatal cap. Counting a throttle as a hard
///    reject aborts a leg for congestion that a moment's pause would clear, and
///    resubmitting into it (Hard's 0.5 s ladder) only deepens the storm.
///  - PositionClosed: a reduce/close order the venue refused because the
///    position is already gone (e.g. MEXC 2009 "position nonexistent or
///    closed") — the leg's goal (flat) is achieved, so end it cleanly. Retrying
///    only loops on the same reject until the cap (live-observed: 20 rejects /
///    ~500 s wasted on one leg).
///  - Hard: anything else — exponential backoff, fatal after a cap.
enum class RejectKind { BenignPostOnlyCross, MinNotional, Permanent, Hard, Throttled, PositionClosed };

/// Thrown by gateway order operations on SYNCHRONOUS venue rejection (HTTP
/// error response). Asynchronous rejections arrive as OrderUpdate events.
class GatewayError final : public std::runtime_error {
public:
    RejectKind kind;

    GatewayError(const RejectKind rejectKind, const std::string &message) : std::runtime_error(message), kind(rejectKind) {}
};

struct InstrumentSpec {
    std::string symbol;
    double tickSize{};
    double qtyStep{};
    double minQty{};
    double maxQty{}; /// 0 = no venue cap
    double minNotional{}; /// venue min order value in quote currency; 0 = unknown
};

struct Quote {
    double bid{};
    double ask{};
    /// Local receipt time (steady clock) — freshness is measured against the
    /// local clock, never the exchange timestamp.
    std::chrono::steady_clock::time_point receivedAt{};

    /// Two-sided, uncrossed, positive book. A just-listed/halted symbol can
    /// push bid=0 or bid>ask; pricing off such a book produces nonsense orders.
    [[nodiscard]] bool sane() const { return bid > 0.0 && ask > 0.0 && bid <= ask; }
};

enum class OrderState {
    Accepted, /// resting at the venue (also re-pushed on successful amend)
    PartiallyFilled,
    Filled,
    Cancelled, /// NOTE: may still carry fills (cancel raced a partial fill)
    Rejected
};

struct OrderUpdate {
    std::string clientOrderId;
    std::string symbol;
    OrderState state{OrderState::Accepted};
    double price{}; /// current order price — reflects amends
    double cumFilledQty{};
    RejectKind rejectKind{RejectKind::Hard}; /// meaningful when state == Rejected
    std::string reason{}; /// venue reject/cancel reason, for logging only
};

struct FillEvent {
    std::string clientOrderId;
    std::string symbol;
    std::string fillId; /// venue execution id — dedup key (events may re-deliver)
    double qty{};
    double price{};
    bool isMaker{false};
};

using onOrderUpdateEvent = std::function<void(const OrderUpdate &)>;
using onFillEvent = std::function<void(const FillEvent &)>;
using onQuoteEvent = std::function<void(const std::string &symbol, const Quote &)>;

class IExecutionGateway {
public:
    virtual ~IExecutionGateway() = default;

    [[nodiscard]] virtual std::string name() const = 0;

    /**
     * Connect the gateway's event streams (private order/fill feed). Blocks
     * until the feed is live or throws. Must be called before any order op.
     */
    virtual void start() = 0;

    /**
     * Instrument metadata. Cached inside the adapter; throws on unknown symbol.
     */
    virtual InstrumentSpec instrumentSpec(const std::string &symbol) = 0;

    /**
     * Drop/refresh the cached instrument metadata. Venues re-parameterize
     * instruments over time (tick size, lot size, min qty), so the strategy
     * calls this at the top of every rebalance/cleanup cycle — an order built
     * from stale steps draws venue rejects ("Price invalid"). Throws on REST
     * failure: better no orders this cycle than orders on stale metadata.
     */
    virtual void refreshInstruments() = 0;

    /**
     * Ensure top-of-book quotes flow for the symbol. Idempotent.
     */
    virtual void subscribeQuotes(const std::string &symbol) = 0;

    /**
     * Stop the quote flow for the symbol (subscriptions must not accumulate
     * over a long-running process). No-op when not subscribed.
     */
    virtual void unsubscribeQuotes(const std::string &symbol) = 0;

    /**
     * Latest top-of-book, or nullopt when nothing was received yet. The caller
     * checks freshness (receivedAt) and sanity (sane()).
     */
    virtual std::optional<Quote> lastQuote(const std::string &symbol) = 0;

    /**
     * Register event sinks — called once, before start(). Callbacks arrive on
     * the gateway's io thread and must be fast and non-blocking.
     */
    virtual void setOrderUpdateCallback(const onOrderUpdateEvent &cb) = 0;

    virtual void setFillCallback(const onFillEvent &cb) = 0;

    /**
     * Register a per-tick top-of-book sink — wakes the chase the moment the
     * book moves instead of waiting out the repost interval. Same io-thread
     * contract as the other callbacks.
     */
    virtual void setQuoteCallback(const onQuoteEvent &cb) = 0;

    /**
     * Submit a post-only (maker-or-reject) limit order. Returns once the venue
     * ACCEPTED the order; throws GatewayError on synchronous rejection. An
     * async post-only-cross rejection still arrives later as an OrderUpdate
     * with state=Rejected / kind=BenignPostOnlyCross.
     * @param clientOrderId caller-generated unique id — the sole order handle
     */
    virtual void submitPostOnlyLimit(const std::string &clientOrderId, const std::string &symbol, OrderSide side, double qty, double price, bool reduceOnly) = 0;

    /**
     * True when the venue supports in-place price amendment. When false, the
     * chase core re-prices via cancel + fresh submit instead of amendPrice.
     */
    [[nodiscard]] virtual bool supportsAmend() const = 0;

    /**
     * Amend the resting order's price in place (keeps venue queue position
     * semantics of the venue). Throws GatewayError when the amend cannot be
     * applied (e.g. order just filled/cancelled, or new price would cross) —
     * the core then falls back to cancel + resubmit.
     */
    virtual void amendPrice(const std::string &clientOrderId, const std::string &symbol, double price) = 0;

    /**
     * Cancel the order.
     * @return true when the venue accepted the cancel (the Cancelled event
     *         follows); false when the order already left the book ("order
     *         not exists / too late") — its terminal event either already
     *         arrived or never will, so the caller must not wait for one.
     * @throws GatewayError on any other failure
     */
    virtual bool cancel(const std::string &clientOrderId, const std::string &symbol) = 0;

    /**
     * Reduce-only MARKET order — the dust-close path. Venues exempt reduce-only
     * closes from the min-notional check (probe-verified on Bybit), which makes
     * sub-min residuals closable at all.
     */
    virtual void submitReduceOnlyMarket(const std::string &clientOrderId, const std::string &symbol, OrderSide side, double qty) = 0;
};

} // namespace stonky::execution

#endif // I_EXECUTION_GATEWAY_H
