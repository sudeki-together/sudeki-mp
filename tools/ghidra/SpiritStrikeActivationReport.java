// Reports Spirit Strike manager entry points and their native callers.
// The script is read-only and refuses to inspect an unexpected executable.
// @category SudekiMP

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;

import java.util.HashSet;
import java.util.Set;

public class SpiritStrikeActivationReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private void decompileFunction(
            DecompInterface decompiler, Function function, String role) {
        DecompileResults result = decompiler.decompileFunction(function, 90, monitor);
        println("");
        println("===== " + role + " function=" + function.getEntryPoint() +
            " " + function.getName(true) + " =====");
        if (!result.decompileCompleted()) {
            println("Decompiler failed: " + result.getErrorMessage());
            return;
        }
        println(result.getDecompiledFunction().getC());
    }

    private void decompileContaining(
            DecompInterface decompiler, long value, String role,
            Set<Address> decompiled) {
        Address target = currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
        Function function = currentProgram.getFunctionManager().getFunctionContaining(target);
        if (function == null) {
            println(role + ": no function at " + target);
            return;
        }
        if (decompiled.add(function.getEntryPoint())) {
            decompileFunction(decompiler, function, role + " target=" + target);
        }
    }

    private void reportSymbol(
            DecompInterface decompiler, Symbol symbol, Set<Address> decompiled) {
        Address target = symbol.getAddress();
        Function function = currentProgram.getFunctionManager().getFunctionAt(target);
        ReferenceIterator references = currentProgram.getReferenceManager()
            .getReferencesTo(target);
        println("");
        println("SYMBOL name=" + symbol.getName(true) + " address=" + target +
            " type=" + symbol.getSymbolType());
        while (references.hasNext()) {
            Reference reference = references.next();
            Function caller = currentProgram.getFunctionManager().getFunctionContaining(
                reference.getFromAddress());
            println("  REF from=" + reference.getFromAddress() + " type=" +
                reference.getReferenceType() + " caller=" +
                (caller == null ? "<none>" : caller.getName(true)));
            if (caller != null && decompiled.add(caller.getEntryPoint())) {
                decompileFunction(decompiler, caller,
                    "code reference to " + symbol.getName(true));
            }
        }
        if (function != null && decompiled.add(function.getEntryPoint())) {
            decompileFunction(decompiler, function, "matching Spirit Strike symbol");
        }
    }

    private void reportAddress(
            DecompInterface decompiler, long value, String role,
            Set<Address> decompiled) {
        Address target = currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
        ReferenceIterator references = currentProgram.getReferenceManager()
            .getReferencesTo(target);
        println("");
        println("ADDRESS role=" + role + " target=" + target);
        while (references.hasNext()) {
            Reference reference = references.next();
            Function caller = currentProgram.getFunctionManager().getFunctionContaining(
                reference.getFromAddress());
            println("  REF from=" + reference.getFromAddress() + " type=" +
                reference.getReferenceType() + " caller=" +
                (caller == null ? "<none>" : caller.getName(true)));
            if (caller != null && decompiled.add(caller.getEntryPoint())) {
                decompileFunction(decompiler, caller,
                    "code reference to " + role);
            }
        }
    }

    @Override
    protected void run() throws Exception {
        String actualSha256 = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(actualSha256)) {
            throw new Exception("Unexpected executable SHA256: " + actualSha256);
        }

        println("SudekiMP Spirit Strike activation report");
        println("SHA256=" + actualSha256);
        println("ImageBase=" + currentProgram.getImageBase());

        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        Set<Address> decompiled = new HashSet<Address>();
        decompileContaining(decompiler, 0x0040fba0L,
            "ForceSpiritStrike implementation", decompiled);
        decompileContaining(decompiler, 0x0040fa30L,
            "Spirit Strike active query", decompiled);
        decompileContaining(decompiler, 0x00410940L,
            "Spirit Strike activation validator", decompiled);
        decompileContaining(decompiler, 0x0040fa40L,
            "Spirit Strike definition lookup", decompiled);
        decompileContaining(decompiler, 0x0040f1a0L,
            "Spirit Strike selected definition lookup", decompiled);
        decompileContaining(decompiler, 0x00411220L,
            "Spirit Strike SSP cost query", decompiled);
        decompileContaining(decompiler, 0x0068dcafL,
            "Main-frame timing/update path", decompiled);
        reportAddress(decompiler, 0x0040fba0L,
            "ForceSpiritStrike implementation", decompiled);
        reportAddress(decompiler, 0x00808d30L,
            "Spirit Strike manager global", decompiled);
        SymbolIterator symbols = currentProgram.getSymbolTable().getAllSymbols(true);
        while (symbols.hasNext() && !monitor.isCancelled()) {
            Symbol symbol = symbols.next();
            String lower = symbol.getName(true).toLowerCase();
            if (lower.contains("forcespiritstrike") ||
                    lower.contains("startspiritstrikeattack") ||
                    lower.contains("getssp") ||
                    lower.contains("isactive@cspiritstrikemanager") ||
                    lower.contains("isspiritstrikeactive") ||
                    lower.contains("spiritstrikeuserenable") ||
                    lower.contains("spiritstrikeuserdisable") ||
                    lower.contains("cspiritstrike::") ||
                    lower.contains("cspiritstrikemanager::vftable") ||
                    lower.contains("sui_spiritstrike") ||
                    lower.contains("uispiritstrikebar") ||
                    lower.contains("spirit_strike_bar_full")) {
                reportSymbol(decompiler, symbol, decompiled);
            }
        }
        decompiler.dispose();
    }
}
