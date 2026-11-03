# TTcards core compatibility layer

This independently written adapter provides fixed-width values, geometry, logging,
bounded seeded randomness, and inert core-only rendering handles. The PXA frontend
renders the real board/cards separately. It does not load or draw legacy assets.

Point and rectangle defaults are negative-one sentinels, matching the original
nomlib definitions. Changing them to zero would alter CPU null-move comparisons.
The RNG is an explicitly seeded rejection-sampled xorshift32 backend; it replaces
the platform RNG, without changing the upstream board/AI decision methods.

JSON persistence, original renderer draw methods, and collection-driven random
hand creation are deliberately omitted from the prepared compilation unit.
The wrapper must deal the selected real card table through CardHand::push_back.
All capture decisions, card ranks, ownership, hand ordering, and AI move selection
remain in the original upstream methods; prepare-engine.py verifies their bodies.

This shim does not implement missing upstream Plus, Same Wall, Sudden Death or
trading rules. Their presence in upstream enums does not establish an implemented
rule. The wrapper should report the original implemented scope accurately.

