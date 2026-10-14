// Decompiles the exact-build functions whose entry VAs are given as script
// arguments (hex, e.g. 4cea30). Read-only: no executable or project mutation.
// @category SudekiMP
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
public class DecompileAtReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";
    @Override
    public void run() throws Exception {
        if (!EXPECTED_SHA256.equals(currentProgram.getExecutableSHA256())) { println("REFUSED unsupported image"); return; }
        DecompInterface d = new DecompInterface(); d.openProgram(currentProgram);
        for (String arg : getScriptArgs()) {
            Address a = currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(Long.parseLong(arg, 16));
            Function f = currentProgram.getFunctionManager().getFunctionContaining(a);
            if (f == null) { println("===== " + arg + " MISSING"); continue; }
            DecompileResults r = d.decompileFunction(f, 120, monitor);
            println("===== " + arg + " function=" + f.getEntryPoint() + " " + f.getName(true) + " =====");
            println(r.decompileCompleted() ? r.getDecompiledFunction().getC() : "Decompiler failed: " + r.getErrorMessage());
        }
        d.dispose();
    }
}
