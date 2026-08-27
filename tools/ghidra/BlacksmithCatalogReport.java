// Reports exact read-only Blacksmith catalog, inventory, equipment, and stat
// seams used by the per-seat preview adapter. Read-only and hash-gated.
// @category SudekiMP

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;

import java.util.HashSet;
import java.util.Set;

public class BlacksmithCatalogReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private Address address(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
    }

    private void decompile(DecompInterface decompiler, Function function,
            String role, Set<Address> seen) {
        if (function == null || !seen.add(function.getEntryPoint())) {
            return;
        }
        DecompileResults result = decompiler.decompileFunction(
            function, 120, monitor);
        println("\n===== " + role + " function=" +
            function.getEntryPoint() + " " + function.getName(true) +
            " =====");
        if (!result.decompileCompleted()) {
            println("Decompiler failed: " + result.getErrorMessage());
            return;
        }
        println(result.getDecompiledFunction().getC());
    }

    private boolean selectedName(String name) {
        return name.contains("BlackSmithAddItem") ||
            name.contains("BlackSmithRemoveItem") ||
            name.contains("BlackSmithClearInventory") ||
            name.contains("BlackSmithSetName") ||
            name.contains("GetShopInventory") ||
            name.contains("GetInventory") ||
            name.contains("GetQuantityByID") ||
            name.contains("GetMoneyAmount") ||
            name.contains("ItemGetPrice") ||
            name.contains("GetCharacterNumberStat") ||
            name.contains("GetCharacterStringStat") ||
            name.contains("GetTextureIconForItem") ||
            name.contains("GetWeaponID");
    }

    @Override
    protected void run() throws Exception {
        String actual = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(actual)) {
            throw new Exception("Unexpected executable SHA256: " + actual);
        }
        println("SudekiMP Blacksmith catalog/equipment report");
        println("SHA256=" + actual);
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        Set<Address> seen = new HashSet<Address>();

        FunctionIterator functions = currentProgram.getFunctionManager()
            .getFunctions(true);
        while (functions.hasNext() && !monitor.isCancelled()) {
            Function function = functions.next();
            String name = function.getName(true);
            if (selectedName(name)) {
                decompile(decompiler, function, "named seam", seen);
            }
        }

        long[] targets = {
            0x0048d6f0L, // UILayerBlackSmith update
            0x0048d970L, // UILayerBlackSmith input
            0x0048e910L, // UILayerBlackSmith render
            0x004904e0L, // state-dependent back transition
            0x00490560L, // page-zero close path
            0x004927c0L, // Blacksmith confirmation path
            0x0048fa50L, // selected party actor/equipment refresh
            0x0048e0e0L, // selected catalog/socket refresh
            0x00492300L, // preview-stat recomputation
            0x004929f0L, // selected native equipment object
            0x004929d0L, // selected augmentation/component identity
            0x004b10d0L, // selected Blacksmith catalog price
            0x0048d280L, // compatibility check
            0x00492830L, // duplicate/availability check
            0x00492910L, // confirmation-page refresh
            0x00492a10L, // previous native socket
            0x00492a80L, // next native socket
            0x004a56a0L, // clear Blacksmith catalog
            0x004b0fe0L, // add Blacksmith catalog entry
            0x004b1060L, // remove Blacksmith catalog entry
            0x00530730L, // inventory augmentation helper
            0x005307e0L, // augmentation compatibility helper
            0x00530850L, // augmentation selected-slot helper
            0x005308a0L, // augmentation current component helper
            0x004220c0L, // shared inventory equipment-byte mutation
            0x004d8790L  // CCharacterWeapon::SetWeapon
        };
        String[] roles = {
            "Blacksmith update", "Blacksmith input", "Blacksmith render",
            "Blacksmith back transition", "Blacksmith close path",
            "Blacksmith confirmation", "selected actor/equipment refresh",
            "catalog/socket refresh",
            "preview-stat recomputation", "selected equipment object",
            "selected component identity", "selected catalog price",
            "compatibility check", "availability check",
            "confirmation-page refresh", "previous socket", "next socket",
            "clear Blacksmith catalog", "add Blacksmith catalog entry",
            "remove Blacksmith catalog entry", "augmentation helper",
            "augmentation compatibility", "selected augmentation slot",
            "current component", "inventory equipment mutation", "SetWeapon"
        };
        for (int index = 0; index < targets.length; ++index) {
            decompile(decompiler,
                currentProgram.getFunctionManager().getFunctionAt(
                    address(targets[index])), roles[index], seen);
        }
        decompiler.dispose();
    }
}
