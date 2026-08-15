// Reports Sudeki's PC Quit-menu state and presentation ownership.
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

public class QuitMenuReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private Address address(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
    }

    private void decompile(
            DecompInterface decompiler,
            Function function,
            String role,
            Set<Address> seen) {
        if (function == null || !seen.add(function.getEntryPoint())) {
            return;
        }
        DecompileResults result = decompiler.decompileFunction(
            function, 90, monitor);
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

    private void reportNamedSymbols(
            DecompInterface decompiler,
            Set<Address> seen) {
        SymbolIterator symbols = currentProgram.getSymbolTable().getAllSymbols(true);
        while (symbols.hasNext() && !monitor.isCancelled()) {
            Symbol symbol = symbols.next();
            String name = symbol.getName(true);
            String lower = name.toLowerCase();
            if (!lower.contains("cpcquitscreen") &&
                !lower.contains("uilayerquitmenu")) {
                continue;
            }
            println("SYMBOL address=" + symbol.getAddress() + " name=" + name +
                " type=" + symbol.getSymbolType());
            Function function = currentProgram.getFunctionManager()
                .getFunctionAt(symbol.getAddress());
            decompile(decompiler, function, "named Quit-menu symbol", seen);
        }
    }

    private void reportDataReferences(
            DecompInterface decompiler,
            Address dataAddress,
            String role,
            Set<Address> seen) {
        println("");
        println("DATA REFERENCES role=" + role + " address=" + dataAddress);
        ReferenceIterator references = currentProgram.getReferenceManager()
            .getReferencesTo(dataAddress);
        while (references.hasNext() && !monitor.isCancelled()) {
            Reference reference = references.next();
            Function function = currentProgram.getFunctionManager()
                .getFunctionContaining(reference.getFromAddress());
            println("  reference=" + reference.getFromAddress() + " function=" +
                (function == null ? "<none>" : function.getEntryPoint() + " " +
                    function.getName(true)));
            decompile(decompiler, function, "reference to " + role, seen);
        }
    }

    private void reportStrings(
            DecompInterface decompiler,
            Set<Address> seen) {
        DataIterator dataIterator = currentProgram.getListing().getDefinedData(true);
        while (dataIterator.hasNext() && !monitor.isCancelled()) {
            Data data = dataIterator.next();
            if (!data.hasStringValue()) {
                continue;
            }
            String value = data.getDefaultValueRepresentation();
            String lower = value.toLowerCase();
            if (!lower.contains("quitmenu") &&
                !lower.contains("sui_pause") &&
                !lower.contains("quitgame") &&
                !lower.contains("exit to windows") &&
                !lower.contains("quit to title")) {
                continue;
            }
            println("");
            println("STRING address=" + data.getAddress() + " value=" + value);
            ReferenceIterator references = currentProgram.getReferenceManager()
                .getReferencesTo(data.getAddress());
            while (references.hasNext() && !monitor.isCancelled()) {
                Reference reference = references.next();
                Function function = currentProgram.getFunctionManager()
                    .getFunctionContaining(reference.getFromAddress());
                println("  reference=" + reference.getFromAddress() + " function=" +
                    (function == null ? "<none>" : function.getEntryPoint() +
                        " " + function.getName(true)));
                decompile(decompiler, function, "Quit-menu string owner", seen);
            }
        }
    }

    @Override
    protected void run() throws Exception {
        String actualSha256 = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(actualSha256)) {
            throw new Exception("Unexpected executable SHA256: " + actualSha256);
        }

        println("SudekiMP Quit-menu report");
        println("SHA256=" + actualSha256);
        println("ImageBase=" + currentProgram.getImageBase());

        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        Set<Address> seen = new HashSet<Address>();
        long[] targets = {
            0x0041dbe0L, // CPCQuitScreenShow(bool)
            0x0041dc00L, // CPCQuitScreenEnable(bool)
            0x0041d690L, // CPCQuitScreen per-frame update/render
            0x0041d700L, // CPCQuitScreen show/hide implementation
            0x0041c920L, // CPCQuitScreen constructor
            0x0041dc20L, // CPCQuitScreen singleton destructor
            0x0041dc50L, // CPCQuitScreen callback destructor
            0x0041dc60L, // CPCQuitScreen callback destructor
            0x005051e0L, // PCQuitGame()
            0x004fd600L, // PauseEverything(bool)
            0x00504be0L, // PauseGame()
            0x00504bf0L, // UnpauseGame()
            0x00427480L, // IsGamePaused@CGameSpeed
            0x004a2740L, // QuitToFrontEnd()
            0x0068d3f0L  // main frame/render loop
        };
        String[] roles = {
            "CPCQuitScreenShow(bool)",
            "CPCQuitScreenEnable(bool)",
            "CPCQuitScreen per-frame update/render",
            "CPCQuitScreen show/hide implementation",
            "CPCQuitScreen constructor",
            "CPCQuitScreen singleton destructor",
            "CPCQuitScreen callback destructor",
            "CPCQuitScreen callback destructor",
            "PCQuitGame()",
            "PauseEverything(bool)",
            "PauseGame()",
            "UnpauseGame()",
            "IsGamePaused@CGameSpeed",
            "QuitToFrontEnd()",
            "main frame/render loop"
        };
        for (int index = 0; index < targets.length; ++index) {
            Function function = currentProgram.getFunctionManager()
                .getFunctionAt(address(targets[index]));
            decompile(decompiler, function, roles[index], seen);
            reportCallers(decompiler, function, roles[index], seen);
        }
        reportDataReferences(
            decompiler,
            address(0x00808d68L),
            "CPCQuitScreen singleton global",
            seen);
        reportNamedSymbols(decompiler, seen);
        reportStrings(decompiler, seen);
        decompiler.dispose();
    }
}
