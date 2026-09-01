// Read-only exact-build report for the four QuickMenu category paths.
// @category SudekiMP

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionManager;

public class QuickMenuCategoryReport extends GhidraScript {
    private static final String SHA =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private Address at(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
    }

    private void report(DecompInterface decompiler, long address, String role)
            throws Exception {
        FunctionManager manager = currentProgram.getFunctionManager();
        Function function = manager.getFunctionContaining(at(address));
        if (function == null) {
            throw new Exception("No function for " + role + " at " + at(address));
        }
        println("==== " + role + " @ " + function.getEntryPoint() +
            " " + function.getName() + " ====");
        DecompileResults results = decompiler.decompileFunction(function, 60,
            monitor);
        if (!results.decompileCompleted()) {
            throw new Exception("Decompile failed: " + role);
        }
        println(results.getDecompiledFunction().getC());
    }

    @Override
    protected void run() throws Exception {
        if (!SHA.equalsIgnoreCase(currentProgram.getExecutableSHA256())) {
            throw new Exception("Unexpected executable image");
        }
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        report(decompiler, 0x00499a80L, "QuickMenu detail/category dispatch");
        report(decompiler, 0x0049a300L, "QuickMenu rebuild/header");
        report(decompiler, 0x0049b3e0L, "QuickMenu skills population");
        report(decompiler, 0x0049b780L, "QuickMenu weapon population");
        report(decompiler, 0x0049cc00L, "QuickMenu item population/start eligibility");
        report(decompiler, 0x00499820L, "QuickMenu action validate/use");
        report(decompiler, 0x0049c0f0L, "QuickMenu item action coordinator");
        report(decompiler, 0x0052e0a0L, "QuickMenu item target validator");
        report(decompiler, 0x0052f440L, "QuickMenu item target policy");
        report(decompiler, 0x004972c0L, "QuickMenu item action failure");
        report(decompiler, 0x00499e40L, "QuickMenu weapon/item detail");
        report(decompiler, 0x0052e1e0L, "QuickMenu weapon validator");
        report(decompiler, 0x0052df60L, "QuickMenu weapon executor");
        report(decompiler, 0x00560580L, "QuickMenu item executor");
        report(decompiler, 0x00421ce0L, "Inventory category item lookup");
        report(decompiler, 0x00421e80L, "Inventory category count");
        report(decompiler, 0x0043f430L, "Inventory accessor");
        decompiler.dispose();
    }
}
