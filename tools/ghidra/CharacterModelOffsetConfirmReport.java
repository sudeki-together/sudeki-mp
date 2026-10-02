// CharacterModelOffsetConfirmReport — decompile the functions that load the
// model from a character/owner to pin the character -> model offset (candidate
// 0x130, seen in FUN_00561960 `MOV EDI,[param+0x130]`), and distinguish it from
// the ranged +0x134 "world" component.
//
// Read-only: refuses any executable other than the supported GOG build.
// @category SudekiMP

import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.listing.Program;

import java.util.LinkedHashSet;
import java.util.Set;

public class CharacterModelOffsetConfirmReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private static final long[] TARGETS = {
        0x00561960L, 0x00588750L, 0x005887a0L, 0x00588470L
    };

    private Address address(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
    }

    private void printDecompile(DecompInterface decompiler, long entry, String tag) {
        Function fn = getFunctionAt(address(entry));
        if (fn == null) {
            println(tag + "_ABSENT 0x" + Long.toHexString(entry));
            return;
        }
        println("");
        println("=== " + tag + " " + fn.getName() + " @0x" +
            Long.toHexString(fn.getEntryPoint().getOffset()) + " ===");
        try {
            DecompileResults results = decompiler.decompileFunction(fn, 120, monitor);
            if (results == null || !results.decompileCompleted()) {
                println("decompile_failed " + fn.getName());
                return;
            }
            println("SIGNATURE " +
                results.getDecompiledFunction().getSignature());
            String[] lines = results.getDecompiledFunction().getC().split("\n");
            for (int index = 0; index < lines.length; ++index) {
                if (index >= 160) {
                    println("  ...TRUNCATED (" + lines.length + " lines total)");
                    break;
                }
                println("  " + lines[index]);
            }
        } catch (Exception error) {
            println("decompile_failed " + fn.getName() + " " + error);
        }
    }

    @Override
    protected void run() throws Exception {
        String actualSha256 = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(actualSha256)) {
            throw new Exception("Unexpected executable SHA256: " + actualSha256);
        }
        println("SudekiMP character -> model offset confirmation");
        println("SHA256=" + actualSha256);

        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        for (long target : TARGETS) {
            printDecompile(decompiler, target, "FUNC");
        }

        // Scan: which functions reference scalar 0x130 (candidate model offset)?
        println("");
        println("=== 0x130 scalar users (candidate character->model offset) ===");
        Set<Function> seen = new LinkedHashSet<Function>();
        Program program = currentProgram;
        ghidra.program.model.listing.Listing listing = program.getListing();
        ghidra.program.model.listing.FunctionIterator fns =
            listing.getFunctions(true);
        while (fns.hasNext()) {
            Function fn = fns.next();
            InstructionIterator it = listing.getInstructions(fn.getBody(), true);
            boolean has130 = false;
            while (it.hasNext()) {
                Instruction ins = it.next();
                for (int i = 0; i < ins.getNumOperands(); ++i) {
                    Object[] objs = ins.getOpObjects(i);
                    for (int j = 0; j < objs.length; ++j) {
                        Object obj = objs[j];
                        if (obj instanceof ghidra.program.model.scalar.Scalar) {
                            long v = ((ghidra.program.model.scalar.Scalar) obj).getUnsignedValue();
                            if (v == 0x130L) {
                                has130 = true;
                            }
                        }
                    }
                }
            }
            if (has130 && seen.add(fn)) {
                println(String.format(
                    "SCALAR_0x130 %s @0x%08X",
                    fn.getName(), fn.getEntryPoint().getOffset()));
            }
        }
        println("SCALAR_0x130_COUNT=" + seen.size());

        decompiler.dispose();
        println("");
        println("REPORT_COMPLETE CharacterModelOffsetConfirmReport");
    }
}
