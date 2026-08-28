// Reports read-only exact GOG shop catalog/open/transaction seams for the
// personal-checkout adapter. It does not modify the program or live game.
// @category SudekiMP

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

import java.util.HashSet;
import java.util.Set;

public class ShopCheckoutReport extends GhidraScript {
    private static final String SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private Address at(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
    }

    private void decompile(DecompInterface decompiler, long value,
            String role, Set<Address> seen) {
        Function function = currentProgram.getFunctionManager()
            .getFunctionAt(at(value));
        println("\n===== " + role + " " + at(value) + " =====");
        if (function == null || !seen.add(function.getEntryPoint())) {
            println(function == null ? "missing=true" : "already_seen=true");
            return;
        }
        DecompileResults result = decompiler.decompileFunction(function, 180,
            monitor);
        println(result.decompileCompleted() ?
            result.getDecompiledFunction().getC() :
            "decompile_error=" + result.getErrorMessage());
    }

    private void callers(long value, String role) {
        ReferenceIterator refs = currentProgram.getReferenceManager()
            .getReferencesTo(at(value));
        int count = 0;
        println("\n===== CALLERS " + role + " " + at(value) + " =====");
        while (refs.hasNext() && !monitor.isCancelled()) {
            Reference reference = refs.next();
            if (!reference.getReferenceType().isCall()) continue;
            Function owner = currentProgram.getFunctionManager()
                .getFunctionContaining(reference.getFromAddress());
            println("callsite=" + reference.getFromAddress() + " owner=" +
                (owner == null ? "<none>" : owner.getEntryPoint() + " " +
                    owner.getName(true)));
            if (++count == 64) {
                println("truncated=true");
                break;
            }
        }
        println("count=" + count);
    }

    @Override
    protected void run() throws Exception {
        String actual = currentProgram.getExecutableSHA256();
        if (!SHA256.equalsIgnoreCase(actual)) {
            throw new Exception("Unexpected executable SHA256: " + actual);
        }
        DecompInterface decompiler = new DecompInterface();
        Set<Address> seen = new HashSet<Address>();
        long[] functions = {
            0x00417d40L, // GetShopInventory
            0x00417c90L, // ShopAddItem(id)
            0x00417c70L, // ShopAddItem(id, quantity)
            0x00417c50L, // ShopClearInventory
            0x00417cb0L, // ShopRemoveItem
            0x00417cd0L, // ShopSetMode
            0x00417cf0L, // ShopSetName(id)
            0x00417d00L, // ShopSetName(text)
            0x00417920L, // catalog add/update helper
            0x00417a30L, // catalog remove helper
            0x00417b30L, // catalog sort helper
            0x00489d70L, // native buy
            0x00489df0L, // native sell
            0x004898a0L, // shop input
            0x0048a0e0L, // selected row refresh
            0x0048c570L, // shop selection gate
            0x0048c6a0L, // selected price/affordability
            0x0048a210L, // shop render
            0x0048d1a0L, // ShopStart
            0x0048d1c0L  // IsShopActive
        };
        String[] roles = {
            "GetShopInventory", "ShopAddItem", "ShopAddItem quantity",
            "ShopClearInventory", "ShopRemoveItem", "ShopSetMode",
            "ShopSetName id", "ShopSetName text", "catalog add/update",
            "catalog remove", "catalog sort", "native buy", "native sell", "shop input",
            "selected row refresh", "selection gate", "price/affordability",
            "shop render", "ShopStart", "IsShopActive"
        };
        decompiler.openProgram(currentProgram);
        for (int index = 0; index < functions.length; ++index) {
            decompile(decompiler, functions[index], roles[index], seen);
            callers(functions[index], roles[index]);
        }
        callers(0x00808d44L, "CShopInventory global");
        decompiler.dispose();
    }
}
