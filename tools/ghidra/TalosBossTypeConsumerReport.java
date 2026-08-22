// Finds exact-build code consumers of the CAiUnit AI Unit Type field (+0x148).
// Read-only; intended to isolate the real-Talos versus clone AI decision guard.
// @category SudekiMP

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.scalar.Scalar;
import java.util.HashSet;
import java.util.Set;

public class TalosBossTypeConsumerReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private boolean hasScalar(Instruction instruction, long value) {
        for (int operand = 0; operand < instruction.getNumOperands(); ++operand) {
            for (Object object : instruction.getOpObjects(operand)) {
                if (object instanceof Scalar &&
                    ((Scalar)object).getUnsignedValue() == value) {
                    return true;
                }
            }
        }
        return false;
    }

    private void decompile(
        DecompInterface decompiler,
        Function function,
        Set<Address> seen
    ) {
        if (function == null || !seen.add(function.getEntryPoint())) {
            return;
        }
        println("\n===== AI_UNIT_TYPE_148 " + function.getEntryPoint() +
            " " + function.getName(true) + " =====");
        DecompileResults results = decompiler.decompileFunction(
            function, 120, monitor);
        println(results.decompileCompleted() ?
            results.getDecompiledFunction().getC() :
            results.getErrorMessage());
    }

    @Override
    protected void run() throws Exception {
        DecompInterface decompiler;
        InstructionIterator instructions;
        Set<Address> seen;

        if (!EXPECTED_SHA256.equalsIgnoreCase(
                currentProgram.getExecutableSHA256())) {
            throw new Exception("Unexpected executable SHA256");
        }
        decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        seen = new HashSet<Address>();
        instructions = currentProgram.getListing().getInstructions(true);
        while (instructions.hasNext() && !monitor.isCancelled()) {
            Instruction instruction = instructions.next();
            Function function;
            if (!hasScalar(instruction, 0x148L)) {
                continue;
            }
            function = currentProgram.getFunctionManager()
                .getFunctionContaining(instruction.getAddress());
            println("match=" + instruction.getAddress() + " text=" +
                instruction.toString() + " owner=" +
                (function == null ? "<none>" : function.getEntryPoint()));
            decompile(decompiler, function, seen);
        }
        println("FUNCTION_COUNT=" + seen.size());
        decompiler.dispose();
    }
}
