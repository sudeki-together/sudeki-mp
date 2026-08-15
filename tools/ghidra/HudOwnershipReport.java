// Reports Sudeki's PC party-HUD construction, update, and character ownership.
// Read-only: refuses to inspect any executable other than the supported GOG build.
// @category SudekiMP

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
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

public class HudOwnershipReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private void decompile(
            DecompInterface decompiler,
            Function function,
            String role,
            Set<Address> seen) {
        if (function == null || !seen.add(function.getEntryPoint())) {
            return;
        }
        DecompileResults result = decompiler.decompileFunction(
            function, 120, monitor);
        println("");
        println("===== " + role + " function=" + function.getEntryPoint() +
            " " + function.getName(true) + " =====");
        if (!result.decompileCompleted()) {
            println("Decompiler failed: " + result.getErrorMessage());
            return;
        }
        println(result.getDecompiledFunction().getC());
    }

    private void reportCallers(
            DecompInterface decompiler,
            Function function,
            String role,
            Set<Address> seen) {
        if (function == null) {
            return;
        }
        println("");
        println("CALLERS role=" + role + " function=" +
            function.getEntryPoint());
        ReferenceIterator references = currentProgram.getReferenceManager()
            .getReferencesTo(function.getEntryPoint());
        while (references.hasNext() && !monitor.isCancelled()) {
            Reference reference = references.next();
            if (!reference.getReferenceType().isCall()) {
                continue;
            }
            Function caller = currentProgram.getFunctionManager()
                .getFunctionContaining(reference.getFromAddress());
            println("  callsite=" + reference.getFromAddress() + " caller=" +
                (caller == null ? "<none>" : caller.getEntryPoint() + " " +
                    caller.getName(true)));
            decompile(decompiler, caller, "caller of " + role, seen);
        }
    }

    private boolean relevantString(String value) {
        String lower = value.toLowerCase();
        return lower.contains("sui_hud_") ||
            lower.contains("hud_%d_portrait") ||
            lower.contains("hud_%d_hpbar") ||
            lower.contains("hud_%d_spbar") ||
            lower.contains("hud_%d_status") ||
            lower.contains("hud_%d_ai") ||
            lower.contains("healthnumber_") ||
            lower.contains("sui_portrait_ailish") ||
            lower.contains("sui_portrait_buki");
    }

    private void reportStrings(
            DecompInterface decompiler,
            Set<Address> seen) {
        DataIterator iterator = currentProgram.getListing().getDefinedData(true);
        while (iterator.hasNext() && !monitor.isCancelled()) {
            Data data = iterator.next();
            if (!data.hasStringValue()) {
                continue;
            }
            String value = data.getDefaultValueRepresentation();
            if (!relevantString(value)) {
                continue;
            }
            println("");
            println("STRING address=" + data.getAddress() + " value=" + value);
            ReferenceIterator references = currentProgram.getReferenceManager()
                .getReferencesTo(data.getAddress());
            while (references.hasNext() && !monitor.isCancelled()) {
                Reference reference = references.next();
                Function owner = currentProgram.getFunctionManager()
                    .getFunctionContaining(reference.getFromAddress());
                println("  reference=" + reference.getFromAddress() +
                    " function=" + (owner == null ? "<none>" :
                        owner.getEntryPoint() + " " + owner.getName(true)));
                decompile(decompiler, owner, "HUD string owner", seen);
                reportCallers(decompiler, owner, "HUD string owner", seen);
            }
        }
    }

    private void reportNamedSymbols(
            DecompInterface decompiler,
            Set<Address> seen) {
        SymbolIterator symbols = currentProgram.getSymbolTable().getAllSymbols(true);
        while (symbols.hasNext() && !monitor.isCancelled()) {
            Symbol symbol = symbols.next();
            String lower = symbol.getName(true).toLowerCase();
            if (!lower.contains("uiportrait") &&
                !lower.contains("hud_") &&
                !lower.contains("healthbar")) {
                continue;
            }
            println("SYMBOL address=" + symbol.getAddress() + " name=" +
                symbol.getName(true) + " type=" + symbol.getSymbolType());
            Function function = currentProgram.getFunctionManager()
                .getFunctionAt(symbol.getAddress());
            decompile(decompiler, function, "named HUD symbol", seen);
        }
    }

    @Override
    protected void run() throws Exception {
        String actualSha256 = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(actualSha256)) {
            throw new Exception("Unexpected executable SHA256: " + actualSha256);
        }
        println("SudekiMP HUD ownership report");
        println("SHA256=" + actualSha256);
        println("ImageBase=" + currentProgram.getImageBase());

        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        Set<Address> seen = new HashSet<Address>();
        reportStrings(decompiler, seen);
        reportNamedSymbols(decompiler, seen);
        decompiler.dispose();
    }
}
