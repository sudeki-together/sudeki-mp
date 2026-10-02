// Decompiles the ANIMID-name-table consumers to determine how a global
// animation id (ANIMID_*) becomes a per-character renderer selector: whether
// the model stores a readable current ANIMID and whether ANIMID -> clip is a
// table lookup in DATA or a per-character code path.
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
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;
import ghidra.program.model.symbol.ReferenceManager;

import java.util.ArrayList;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;

public class AnimIdResolutionReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private static final long ANIMID_NAME_TABLE = 0x007363e8L;
    private static final int MAX_DECOMPILE_LINES = 90;

    /** Seeded from CharacterAnimationBankShapeReport (Part A), so this report is
     *  self-contained and does not depend on the earlier run's output. */
    private static final long[] ANIMID_CONSUMERS = new long[] {
        0x0043AF10L, // TsaGetCurrentAnimation (CNewGameModelAnimation)
        0x004198E0L,
        0x00414810L,
        0x0042D6B0L,
        0x004CE410L,
        0x0052AB20L,
        0x0052AB80L,
        0x0052D3F0L,
        0x00545320L,
        0x00544B50L,
        0x00587A80L,
        0x00589630L
    };

    private Address address(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
    }

    private boolean referencesAnimIdNameTable(Address entry) {
        Function fn = getFunctionAt(entry);
        if (fn == null) {
            return false;
        }
        ReferenceManager manager = currentProgram.getReferenceManager();
        ReferenceIterator iterator =
            manager.getReferencesTo(address(ANIMID_NAME_TABLE));
        while (iterator.hasNext()) {
            Reference reference = iterator.next();
            Function owner = getFunctionContaining(reference.getFromAddress());
            if (owner != null && owner.getEntryPoint().equals(entry)) {
                return true;
            }
        }
        return false;
    }

    private void printDecompile(DecompInterface decompiler, long entry) {
        Function fn = getFunctionAt(address(entry));
        if (fn == null) {
            println("FUNCTION_ABSENT 0x" + Long.toHexString(entry));
            return;
        }
        println("");
        println("=== FUNC " + fn.getName() + " @0x" +
            Long.toHexString(fn.getEntryPoint().getOffset()) + " ===");
        try {
            DecompileResults results = decompiler.decompileFunction(fn, 90, monitor);
            if (results == null || !results.decompileCompleted()) {
                println("decompile_failed " + fn.getName());
                return;
            }
            String[] lines =
                results.getDecompiledFunction().getC().split("\n");
            println("SIGNATURE " + results.getDecompiledFunction()
                .getSignature());
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

    private void printCalleeCensus(long entry) {
        Function fn = getFunctionAt(address(entry));
        if (fn == null) {
            return;
        }
        println("");
        println("--- CALLEES of " + fn.getName() + " @0x" +
            Long.toHexString(entry));
        Set<Long> seen = new LinkedHashSet<Long>();
        InstructionIterator iterator =
            currentProgram.getListing().getInstructions(fn.getBody(), true);
        while (iterator.hasNext()) {
            Instruction instruction = iterator.next();
            if (!instruction.getFlowType().isCall()) {
                continue;
            }
            Address[] flows = instruction.getFlows();
            for (int index = 0; index < flows.length; ++index) {
                long target = flows[index].getOffset();
                if (!seen.add(target)) {
                    continue;
                }
                Function callee = getFunctionAt(flows[index]);
                println(String.format(
                    "  CALLEE 0x%08X %s animid_table_ref=%s",
                    target,
                    callee == null ? "<unresolved>" : callee.getName(),
                    referencesAnimIdNameTable(flows[index]) ? "yes" : "no"));
            }
        }
        println("CALLEE_COUNT " + fn.getName() + "=" + seen.size());
    }

    @Override
    protected void run() throws Exception {
        String actualSha256 = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(actualSha256)) {
            throw new Exception("Unexpected executable SHA256: " + actualSha256);
        }
        println("SudekiMP ANIMID resolution report");
        println("SHA256=" + actualSha256);
        println("EXE=" + currentProgram.getExecutablePath());
        println("CONSUMER_COUNT=" + ANIMID_CONSUMERS.length);
        println("");

        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        // Part A: full decompilation of every ANIMID-name-table consumer.
        println("=== PART A: decompiled ANIMID consumers ===");
        for (int index = 0; index < ANIMID_CONSUMERS.length; ++index) {
            printDecompile(decompiler, ANIMID_CONSUMERS[index]);
        }

        // Part B: callee census, flagging callees that also touch ANIMIDs.
        println("");
        println("=== PART B: callee census ===");
        for (int index = 0; index < ANIMID_CONSUMERS.length; ++index) {
            printCalleeCensus(ANIMID_CONSUMERS[index]);
        }

        // Part C: decompile the ANIMID-touching callees (the resolution layer,
        // one hop below the consumers).
        println("");
        println("=== PART C: ANIMID-touching callees (one hop down) ===");
        Set<Long> secondHop = new LinkedHashSet<Long>();
        for (int index = 0; index < ANIMID_CONSUMERS.length; ++index) {
            Function fn = getFunctionAt(address(ANIMID_CONSUMERS[index]));
            if (fn == null) {
                continue;
            }
            InstructionIterator iterator =
                currentProgram.getListing().getInstructions(fn.getBody(), true);
            while (iterator.hasNext()) {
                Instruction instruction = iterator.next();
                if (!instruction.getFlowType().isCall()) {
                    continue;
                }
                Address[] flows = instruction.getFlows();
                for (int inner = 0; inner < flows.length; ++inner) {
                    if (referencesAnimIdNameTable(flows[inner])) {
                        secondHop.add(flows[inner].getOffset());
                    }
                }
            }
        }
        println("SECOND_HOP_COUNT=" + secondHop.size());
        for (Long entry : secondHop) {
            printDecompile(decompiler, entry.longValue());
        }

        decompiler.dispose();
        println("");
        println("REPORT_COMPLETE AnimIdResolutionReport");
    }
}
