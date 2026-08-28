# Shared inventory and personal wallets

This is the policy contract for a future cooperative economy adapter. The
implementation is an intentionally native-inert foundation:

- `src/engine/personal_wallet.c` owns the four persistent wallet balances and
  party reserve.
- `src/engine/shared_inventory_entitlement_ledger.c` owns eligible
  consumable/material ownership while preserving one native shared stack.
- `src/engine/economy_coordinator.c` serializes a request through one verified
  external mutation.
- `src/engine/economy_sidecar.c` encodes a versioned, checksummed snapshot
  bound to a native save identity.
- `src/engine/merchant_checkout.c` provides the authenticated, per-character
  catalog/selection/quote and verified-purchase contract for the future shop
  presentation.

None of these modules calls Sudeki, installs a hook, alters a save, or enables
shop/blacksmith checkout. Live routing stays disabled until the buyer and
merchant provenance path is complete.

## Ownership and money policy

Wallets are owned by Sudeki's four stable character resource identities—Tal
`0x23`, Ailish `0x01`, Buki `0x05`, and Elco `0x0e`—not by controller seats. A wallet therefore follows its character
through drop-out, AI takeover, controller reassignment, and rejoining. Each
character wallet caps independently at 99,999 florins; credit above that cap
is reported in the transaction plan and receipt, then discarded.

| Event | Money result | Required external proof |
| --- | --- | --- |
| Literal florin pickup | Credit the full amount to all four wallets | Exact drop/source was consumed |
| Quest/script/story reward | Credit the full amount to all four wallets | Exact script/reward source was classified |
| Purchase | Debit the buyer's character wallet | Exact shared item was added and assigned to buyer |
| Forge | Debit the builder's character wallet | Exact shared equipment was changed |
| Sell a shared item | Credit the **full sale price to all four character wallets** | Exactly one selected shared item was removed |

A sale is intentionally a four-way dividend, not a split. The transaction
stores an explicit native quantity, while its amount is the total proceeds for
that quantity. Selling an item for 100 produces nominal character credit of 400: +100 Tal, +100 Ailish,
+100 Buki, and +100 Elco. It does not credit the reserve. Characters receive
the dividend even when they are not currently human-controlled. The selected
quantity is removed exactly once by the future native adapter.

The native party reserve also caps at 99,999; reserve overflow is reported and
discarded. Existing single-player money migrates once into this reserve.
Migration does not duplicate old money into every personal wallet. The host may
move reserve funds to one chosen character only through the future party-
inventory screen, outside combat. Snapshot entries are keyed by stable
character ID, so serialized array ordering cannot move money between
characters.

## Eligible shared inventory ownership

The physical `CInventory` remains one native inventory; there is no stock
multiplier, duplicated item, or raised stack cap. The entitlement ledger has an
explicit registration allowlist. Until an item is registered, it remains purely
native shared. Every registered entry enforces:

`native quantity = Tal + Ailish + Buki + Elco + unallocated`

Pickups and purchases add the resulting entitlement to the proven collector or
buyer. Transfers change only the sidecar entitlement—not the native stack.
Only the owner may consume or sell their units. Any mismatch in the observed
native quantity quarantines the ledger and disables economy mutations instead
of silently assigning an owner. Keys, quest/story items, and equipment are
intentionally excluded from this first allowlist.

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

The coordinator records a stable character, operation serial, wallet and
inventory generations, source/merchant provenance, and native before/after
quantity. It permits a single verified add/remove/forge effect; a stale,
partial, unknown, or contradictory result quarantines the coordinator.

The sidecar contains pointer-free wallet and entitlement snapshots plus the
native save identity. A missing sidecar is the one-time native-money-to-reserve
migration path. A corrupt or mismatched sidecar must leave the native save
untouched and keep economy mutation disabled pending recovery.

## Deliberate activation gate

This is not yet a live co-op economy. Native shop and blacksmith paths are
global and write native money directly, so they cannot be used for personal
wallet checkout. The remaining integration milestone is a native-styled,
per-player party inventory/merchant/forge surface that authenticates the
initiating character, displays that character's funds, calls the coordinator,
and verifies the native effect. Until then, no current shop, blacksmith,
pickup, or save hook is redirected through this foundation.

The merchant core already permits several seats to retain independent catalog
selections while serializing the actual commit. It requires a pointer-free
merchant/catalog snapshot and a current actor-generation lease; it calculates
the buyer's total, debits only that buyer on proof of one native item add, and
assigns the eligible entitlement to that buyer. It is deliberately not wired
to the singleton native `UILayerShop` yet: the next integration work is a
validated catalog reader and an authenticated merchant interaction source.

The catalog reader now exists as a read-only, exact-image-gated adapter. It
copies the bounded native buy listing into a pointer-free snapshot, verifies
every item-definition identity and unit price, and fingerprints the ordered
listing. It deliberately requires a nonzero merchant provenance supplied by a
separate interaction adapter: the singleton `CShopInventory` stores a catalog
and buy/sell mode, not the merchant actor that produced it.

`EnableMerchantCheckoutTracePrototype` is the separate, disabled-by-default
adapter that supplies that missing interaction evidence. It reads the exact
SOL opcode only after the existing interaction trace has authenticated the
native P1 interaction, and accepts only the supported `ShopStart|B` binding
(`0x7FF08FB5`, stored immediately before the name in `WINSOLM.gex`).
Its output is an ephemeral merchant token; it cannot open a shop, intercept
native controls, or perform a checkout. It refuses to install beside the
broad SkillTrace opcode observer rather than chaining an unknown handler.

When that trusted `ShopStart` call returns, the same passive adapter captures
the native buy list into scalar `{item_id, unit_price, listed_quantity}` rows
and fingerprints it. The snapshot is returned only while it still matches the
same SOL/actor/merchant token. This is the data source for independent seat
selection; it still does not route shop input or enable any transaction.
