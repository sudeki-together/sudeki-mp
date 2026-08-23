// Reports the exact-build boss-bar binding, percentage limits, and HUD update
// ownership. Read-only; refuses unsupported executables.
// @category SudekiMP

import ghidra.app.decompiler.DecompileResults;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.DataIterator;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;

import java.util.HashSet;
import java.util.Set;

public class BossBarHealthReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";
    private static final long[] TARGETS = {
        0x004a7080L, // BossBarSetBoss
        0x004a70b0L, // BossBarSetTexture
        0x004a70d0L, // BossBarSetUpperLimitPercentage
        0x004a70f0L, // BossBarSetLowerLimitPercentage
        0x004a6850L, // boss-bar state/setup
        0x004a6d20L, // UILayerBossBar asset setup
        0x004a6ee0L, // UILayerBossBar boss binding
        0x004a6f40L, // UILayerBossBar texture binding
        0x004a7110L, // boss-bar global init/access helper
        0x004a71c0L, // boss-bar UI element setup
        0x00582230L, // native bar fill submission
        0x004b0060L, // CStatDisplayManager::SetStatDispFlags
        0x004b0070L, // CStatDisplayManager::ClrStatDispFlags
        0x004b0080L, // CStatDisplayManager::Enable
        0x004b0090L, // GetStatDisplayManager
        0x004b00a0L, // StatDisplayEnable
        0x00529360L, // CStatDisplay update
        0x00529780L  // CStatDisplay health callback
    };

    private Address address(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
    }

    private void decompile(
            DecompInterface decompiler,
            Function function,
            String role,
            Set<Address> seen) {
        DecompileResults result;
        if (function == null || !seen.add(function.getEntryPoint())) {
            return;
        }
        println("\n===== " + role + " " + function.getEntryPoint() + " " +
            function.getName(true) + " =====");
        result = decompiler.decompileFunction(function, 120, monitor);
        println(result.decompileCompleted() ?
            result.getDecompiledFunction().getC() : result.getErrorMessage());
    }

    private void callers(
            DecompInterface decompiler,
            Function function,
            String role,
            Set<Address> seen) {
        ReferenceIterator references;
        if (function == null) {
            return;
        }
        references = currentProgram.getReferenceManager()
            .getReferencesTo(function.getEntryPoint());
        while (references.hasNext() && !monitor.isCancelled()) {
            Reference reference = references.next();
            Function caller;
            if (!reference.getReferenceType().isCall()) {
                continue;
            }
            caller = currentProgram.getFunctionManager()
                .getFunctionContaining(reference.getFromAddress());
            println("CALL role=" + role + " site=" +
                reference.getFromAddress() + " caller=" +
                (caller == null ? "<none>" : caller.getEntryPoint()));
            decompile(decompiler, caller, "CALLER_OF_" + role, seen);
        }
    }

    private boolean relevant(String value) {
        String lower = value.toLowerCase();
        return lower.contains("hud_boss") ||
            lower.contains("sui_hud_boss") ||
            lower.contains("bossbar") ||
            lower.contains("healthbar_top") ||
            lower.contains("cstatdisplay") ||
            lower.contains("hpbarheight") ||
            lower.contains("stat bar offset");
    }

    @Override
    protected void run() throws Exception {
        DecompInterface decompiler;
        Set<Address> seen;
        if (!EXPECTED_SHA256.equalsIgnoreCase(
                currentProgram.getExecutableSHA256())) {
            throw new Exception("Unexpected executable SHA256");
        }
        decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        seen = new HashSet<Address>();

        for (long targetValue : TARGETS) {
            Function target = currentProgram.getFunctionManager()
                .getFunctionAt(address(targetValue));
            decompile(decompiler, target, "TARGET", seen);
            callers(decompiler, target, "TARGET", seen);
        }

        long[] vtables = {
            0x006cb49cL,
            0x006cb4f0L,
            0x006d21e4L,
            0x006d2224L
        };
        for (long tableValue : vtables) {
            Address table = address(tableValue);
            println("VTABLE address=" + table);
            for (int slot = 0; slot < 0x14; ++slot) {
                Address entry = table.add(slot * 4L);
                long targetValue = Integer.toUnsignedLong(
                    getInt(entry));
                Function target = currentProgram.getFunctionManager()
                    .getFunctionAt(address(targetValue));
                println("  VTABLE_SLOT offset=0x" +
                    Integer.toHexString(slot * 4) + " target=" +
                    address(targetValue) + " function=" +
                    (target == null ? "<none>" : target.getName(true)));
                decompile(decompiler, target, "VTABLE_METHOD", seen);
            }
        }

        DataIterator data = currentProgram.getListing().getDefinedData(true);
        while (data.hasNext() && !monitor.isCancelled()) {
            Data item = data.next();
            if (!item.hasStringValue() ||
                !relevant(item.getDefaultValueRepresentation())) {
                continue;
            }
            println("STRING address=" + item.getAddress() + " value=" +
                item.getDefaultValueRepresentation());
            ReferenceIterator references = currentProgram.getReferenceManager()
                .getReferencesTo(item.getAddress());
            while (references.hasNext()) {
                Reference reference = references.next();
                Function owner = currentProgram.getFunctionManager()
                    .getFunctionContaining(reference.getFromAddress());
                println("  STRING_REF site=" + reference.getFromAddress() +
                    " owner=" + (owner == null ? "<none>" :
                        owner.getEntryPoint()));
                decompile(decompiler, owner, "STRING_OWNER", seen);
                callers(decompiler, owner, "STRING_OWNER", seen);
            }
        }

        SymbolIterator symbols = currentProgram.getSymbolTable()
            .getAllSymbols(true);
        while (symbols.hasNext() && !monitor.isCancelled()) {
            Symbol symbol = symbols.next();
            String name = symbol.getName(true).toLowerCase();
            if (!name.contains("uilayerbossbar") &&
                !name.contains("bossbarset") &&
                !name.contains("cstatdisplay")) {
                continue;
            }
            println("SYMBOL address=" + symbol.getAddress() + " name=" +
                symbol.getName(true));
            decompile(decompiler,
                currentProgram.getFunctionManager().getFunctionAt(
                    symbol.getAddress()), "NAMED_SYMBOL", seen);
        }
        println("FUNCTION_COUNT=" + seen.size());
        decompiler.dispose();
    }
}
