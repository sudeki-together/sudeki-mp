// Exact-build, read-only localization-manager constructor identity report.
// @category SudekiMP

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;

public class LocalizationIdentityReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private Address address(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
    }

    @Override
    protected void run() throws Exception {
        String actual = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(actual)) {
            throw new Exception("Unexpected executable SHA256: " + actual);
        }
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        long[] targets = {0x00530cc0L, 0x0052a9b0L};
        for (long target : targets) {
            Function function = currentProgram.getFunctionManager()
                .getFunctionAt(address(target));
            DecompileResults result = decompiler.decompileFunction(
                function, 120, monitor);
            println("===== " + function.getEntryPoint() + " " +
                function.getName(true) + " =====");
            println(result.getDecompiledFunction().getC());
        }
        SymbolIterator symbols = currentProgram.getSymbolTable()
            .getAllSymbols(true);
        while (symbols.hasNext() && !monitor.isCancelled()) {
            Symbol symbol = symbols.next();
            String lower = symbol.getName(true).toLowerCase();
            if (lower.contains("localisation") ||
                    lower.contains("localization") ||
                    lower.contains("stringtable") ||
                    lower.contains("localisedstring")) {
                println("symbol " + symbol.getAddress() + " " +
                    symbol.getName(true));
            }
        }
        decompiler.dispose();
    }
}
