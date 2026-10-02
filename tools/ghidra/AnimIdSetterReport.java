// Locates the ANIMID write path: the code that sets the per-instance semantic
// animation id at *(byte *)(*(int *)(model + 0xF8) + 2) -- the inverse of
// CNewGameModelAnimation::TsaGetCurrentAnimation (0x43AF10).
//
// Strategy: a full-instruction scan for functions that touch displacement 0xF8
// AND contain a small-displacement memory write (the +2 byte), then decompile
// the intersection so the real C is readable. Also reports the 0x131==3 guard
// sites and the 0xC4 ANIMID bound sites.
//
// Read-only: refuses any executable other than the supported GOG build.
// @category SudekiMP

import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.scalar.Scalar;

import java.util.LinkedHashSet;
import java.util.Set;

public class AnimIdSetterReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private static final long ANIM_ID_STATE_OFFSET = 0xF8L; // -> record, +2 = ANIMID
    private static final long ANIM_ID_BYTE_OFFSET = 0x2L;
    private static final long ANIMATION_STATE_GUARD = 0x131L; // == 3 in the getter
    private static final long ANIMID_COUNT = 0xC4L;
    private static final int MAX_DECOMPILE_LINES = 70;
    private static final int MAX_CANDIDATES = 12;

    private Address address(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
    }

    /** True if any operand carries the given scalar literal. */
    private boolean hasScalar(Instruction instruction, long value) {
        int operands = instruction.getNumOperands();
        for (int index = 0; index < operands; ++index) {
            Object[] objects = instruction.getOpObjects(index);
            for (int inner = 0; inner < objects.length; ++inner) {
                if (objects[inner] instanceof Scalar &&
                    ((Scalar)objects[inner]).getUnsignedValue() == value) {
                    return true;
                }
            }
        }
        return false;
    }

    /** True if operand 0 looks like a small-displacement memory destination,
     *  i.e. a write through a register with a tiny offset -- the +2 byte. */
    private boolean writesSmallDisplacement(Instruction instruction) {
        String mnemonic = instruction.getMnemonicString();
        if (mnemonic == null || !mnemonic.startsWith("MOV")) {
            return false;
        }
        Object[] objects = instruction.getOpObjects(0);
        if (objects.length < 2) {
            return false;
        }
        boolean hasRegister = false;
        boolean hasDisplacement = false;
        for (int index = 0; index < objects.length; ++index) {
            if (objects[index] instanceof ghidra.program.model.lang.Register) {
                hasRegister = true;
            }
            if (objects[index] instanceof Scalar) {
                long value = ((Scalar)objects[index]).getUnsignedValue();
                if (value == ANIM_ID_BYTE_OFFSET) {
                    hasDisplacement = true;
                }
            }
        }
        return hasRegister && hasDisplacement;
    }

    private void printDecompile(DecompInterface decompiler, Function fn) {
        println("");
        println("=== CANDIDATE FUNC " + fn.getName() + " @0x" +
            Long.toHexString(fn.getEntryPoint().getOffset()) + " ===");
        try {
            DecompileResults results = decompiler.decompileFunction(fn, 90, monitor);
            if (results == null || !results.decompileCompleted()) {
                println("decompile_failed " + fn.getName());
                return;
            }
            println("SIGNATURE " +
                results.getDecompiledFunction().getSignature());
            String[] lines =
                results.getDecompiledFunction().getC().split("\n");
            for (int index = 0; index < lines.length; ++index) {
                if (index >= MAX_DECOMPILE_LINES) {
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
        println("SudekiMP ANIMID setter report");
        println("SHA256=" + actualSha256);

        Set<Long> touchesStateOffset = new LinkedHashSet<Long>();
        Set<Long> writesSmallDisp = new LinkedHashSet<Long>();
        Set<Long> guardSites = new LinkedHashSet<Long>();
        Set<Long> boundSites = new LinkedHashSet<Long>();
        long scanned = 0L;

        FunctionIterator functions =
            currentProgram.getFunctionManager().getFunctions(true);
        while (functions.hasNext()) {
            Function fn = functions.next();
            long entry = fn.getEntryPoint().getOffset();
            InstructionIterator iterator =
                currentProgram.getListing().getInstructions(fn.getBody(), true);
            while (iterator.hasNext()) {
                Instruction instruction = iterator.next();
                ++scanned;
                if (hasScalar(instruction, ANIM_ID_STATE_OFFSET)) {
                    touchesStateOffset.add(entry);
                }
                if (writesSmallDisplacement(instruction)) {
                    writesSmallDisp.add(entry);
                }
                if (hasScalar(instruction, ANIMATION_STATE_GUARD)) {
                    guardSites.add(entry);
                }
                if (hasScalar(instruction, ANIMID_COUNT)) {
                    boundSites.add(entry);
                }
            }
        }
        println("INSTRUCTIONS_SCANNED=" + scanned);
        println("TOUCHES_0xF8_COUNT=" + touchesStateOffset.size());
        println("WRITES_SMALL_DISP_COUNT=" + writesSmallDisp.size());
        println("GUARD_0x131_SITES=" + guardSites.size());
        println("BOUND_0xC4_SITES=" + boundSites.size());
        println("");

        Set<Long> candidates = new LinkedHashSet<Long>();
        for (Long entry : touchesStateOffset) {
            if (writesSmallDisp.contains(entry)) {
                candidates.add(entry);
            }
        }
        println("=== INTERSECTION candidates (0xF8 + small-disp write) = " +
            candidates.size() + " ===");
        for (Long entry : candidates) {
            Function fn = getFunctionAt(address(entry.longValue()));
            println(String.format(
                "CANDIDATE 0x%08X %s guard_0x131=%s bound_0xc4=%s",
                entry.longValue(),
                fn == null ? "<unresolved>" : fn.getName(),
                guardSites.contains(entry) ? "yes" : "no",
                boundSites.contains(entry) ? "yes" : "no"));
        }
        println("");

        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        int printed = 0;
        for (Long entry : candidates) {
            if (printed >= MAX_CANDIDATES) {
                println("CANDIDATE_DECOMPILE_TRUNCATED at " + MAX_CANDIDATES);
                break;
            }
            Function fn = getFunctionAt(address(entry.longValue()));
            if (fn != null) {
                printDecompile(decompiler, fn);
                ++printed;
            }
        }

        // The guard sites and bound sites are printed by name for follow-up,
        // since the setter likely shares the getter's 0x131==3 precondition.
        println("");
        println("=== 0x131 guard sites ===");
        for (Long entry : guardSites) {
            Function fn = getFunctionAt(address(entry.longValue()));
            println(String.format(
                "GUARD 0x%08X %s",
                entry.longValue(),
                fn == null ? "<unresolved>" : fn.getName()));
        }
        println("");
        println("=== 0xC4 ANIMID bound sites ===");
        for (Long entry : boundSites) {
            Function fn = getFunctionAt(address(entry.longValue()));
            println(String.format(
                "BOUND 0x%08X %s",
                entry.longValue(),
                fn == null ? "<unresolved>" : fn.getName()));
        }

        decompiler.dispose();
        println("");
        println("REPORT_COMPLETE AnimIdSetterReport");
    }
}
