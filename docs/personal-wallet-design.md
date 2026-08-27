# Character wallets and shared-sale dividends

This is the policy contract for a future cooperative economy adapter. The
current implementation in `src/engine/personal_wallet.c` is a pure,
pointer-free state machine. It does not call Sudeki, install hooks, alter a
save, or enable the experimental shop/blacksmith commit paths.

## Ownership and money policy

Wallets are owned by Sudeki's four stable character resource identities—Tal
`0x23`, Ailish `0x01`, Buki `0x05`, and Elco `0x0e`—not by controller seats. A wallet therefore follows its character
through drop-out, AI takeover, controller reassignment, and rejoining. Each
character wallet caps independently at 99,999 florins; credit above that cap
is reported in the transaction plan and receipt, then discarded.

| Event | Money result | Required external proof |
| --- | --- | --- |
| Literal florin / actor-owned reward | Credit the collecting character | Exact reward source was consumed |
| Anonymous quest or script reward | Credit the party reserve | No shared-item mutation |
| Purchase | Debit the buyer's character wallet | Exact shared item was added |
| Forge | Debit the builder's character wallet | Exact shared equipment was changed |
| Sell a shared item | Credit the **full sale price to all four character wallets** | Exactly one selected shared item was removed |

A sale is intentionally a four-way dividend, not a split. The transaction
stores an explicit native quantity, while its amount is the total proceeds for
that quantity. Selling an item for 100 produces nominal character credit of 400: +100 Tal, +100 Ailish,
+100 Buki, and +100 Elco. It does not credit the reserve. Characters receive
the dividend even when they are not currently human-controlled. The selected
quantity is removed exactly once by the future native adapter.

The native party reserve also caps at 99,999; reserve overflow is reported and
discarded. Existing single-player money migrates once into the party reserve. Migration
does not duplicate old money into every personal wallet. Snapshot entries are
keyed by stable character ID, so serialized array ordering cannot move money
between characters.

## Transaction boundary

The core uses `READY -> PLANNED -> APPLYING -> READY`. Planning computes all
credits, debits, cap overflow, and the required external effect without
changing a balance. A future game-thread adapter must revalidate the stable
subject and generations, claim the plan, perform exactly one native effect,
and report one of three outcomes:

- `NOT_APPLIED`: proof that no external mutation occurred; discard the plan.
- `VERIFIED`: proof of the exact requested effect and a matching generation
  change; apply the precomputed wallet deltas once.
- `AMBIGUOUS`: any possible partial effect; quarantine without moving money.

Contradictory proof also quarantines. Recovery requires a trusted, validated
snapshot. Operation serials are nonzero and strictly increase per save
identity. Replaying an applied serial returns its receipt without changing a
balance; older or consumed serials fail stale. Wallet generations similarly
prevent a plan based on old balances from committing. Ordinary snapshot
restore is rejected while a transaction is planned or applying, so a load
cannot erase an external effect that may still need resolution.

This boundary deliberately leaves native item removal, pickup ownership,
merchant provenance, save serialization, and UI presentation unwired until
their engine seams are proven independently.
