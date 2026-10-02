// Finds how an ANIMID becomes the animation-definition pointer passed to the
// play entry point FUN_004e4f50 (0x4E4F50): lists its callers, shows how each
// builds the definition argument, and decompiles them.
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

public class AnimIdDefinitionLookupReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private static final long ANIM_PLAY = 0x004E4F50L;      // channel, f1, def, f2, time
    private static final long ANIM_STATE_ALLOC = 0x004E1680L;
    private static final long CURRENT_ANIM_GETTER = 0x0043AF10L;
    private static final int MAX_DECOMPILE_LINES = 80;
    private static final int MAX_CALLERS_DECOMPILED = 5;

    private Address address(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
    }

    private Set<Function> callersOf(long target) {
        Set<Function> result = new LinkedHashSet<Function>();
        ReferenceManager manager = currentProgram.getReferenceManager();
        ReferenceIterator iterator = manager.getReferencesTo(address(target));
        while (iterator.hasNext()) {
            Reference reference = iterator.next();
            if (!reference.getReferenceType().isCall()) {
                continue;
            }
            Function owner = getFunctionContaining(reference.getFromAddress());
            if (owner != null) {
                result.add(owner);
            }
        }
        return result;
    }

    /** Print each callsite of target inside fn with the preceding instructions,
     *  so the argument construction (definition pointer) is visible. */
    private void reportCallsites(Function fn, long target, int lookback) {
        List<String> window = new ArrayList<String>();
        InstructionIterator iterator =
            currentProgram.getListing().getInstructions(fn.getBody(), true);
        while (iterator.hasNext()) {
            Instruction instruction = iterator.next();
            window.add(String.format(
                "0x%08X %s %s",
                instruction.getAddress().getOffset(),
                instruction.getMnemonicString(),
                instruction.toString()));
            if (window.size() > lookback) {
                window.remove(0);
            }
            if (!instruction.getFlowType().isCall()) {
                continue;
            }
            Address[] flows = instruction.getFlows();
            boolean matches = false;
            for (int index = 0; index < flows.length; ++index) {
                if (flows[index].getOffset() == target) {
                    matches = true;
                }
            }
            if (!matches) {
                continue;
            }
            println("  CALLSITE 0x" +
                Long.toHexString(fn.getEntryPoint().getOffset()) + " " +
                fn.getName());
            for (int index = 0; index < window.size(); ++index) {
                println("    " + window.get(index));
            }
            println("");
        }
    }

    private void printDecompile(DecompInterface decompiler, Function fn) {
        println("");
        println("=== CALLER FUNC " + fn.getName() + " @0x" +
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
        println("SudekiMP ANIMID definition-lookup report");
        println("SHA256=" + actualSha256);
        println("");

        Set<Function> playCallers = callersOf(ANIM_PLAY);
        println("=== PART A: callers of the play entry point ===");
        println("ANIM_PLAY_CALLER_COUNT=" + playCallers.size());
        for (Function fn : playCallers) {
            println(String.format(
                "ANIM_PLAY_CALLER %s @0x%08X",
                fn.getName(), fn.getEntryPoint().getOffset()));
        }
        println("");

        println("=== PART B: callsites (argument construction) ===");
        for (Function fn : playCallers) {
            reportCallsites(fn, ANIM_PLAY, 14);
        }

        println("=== PART C: callers of the state allocator / animid getter ===");
        Set<Function> allocCallers = callersOf(ANIM_STATE_ALLOC);
        println("ANIM_STATE_ALLOC_CALLER_COUNT=" + allocCallers.size());
        for (Function fn : allocCallers) {
            println(String.format(
                "ANIM_STATE_ALLOC_CALLER %s @0x%08X",
                fn.getName(), fn.getEntryPoint().getOffset()));
        }
        Set<Function> getterCallers = callersOf(CURRENT_ANIM_GETTER);
        println("CURRENT_ANIM_GETTER_CALLER_COUNT=" + getterCallers.size());
        for (Function fn : getterCallers) {
            println(String.format(
                "CURRENT_ANIM_GETTER_CALLER %s @0x%08X",
                fn.getName(), fn.getEntryPoint().getOffset()));
        }
        println("");

        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        println("=== PART D: decompiled play-entry callers ===");
        int printed = 0;
        for (Function fn : playCallers) {
            if (printed >= MAX_CALLERS_DECOMPILED) {
                println("CALLER_DECOMPILE_TRUNCATED at " +
                    MAX_CALLERS_DECOMPILED);
                break;
            }
            printDecompile(decompiler, fn);
            ++printed;
        }
        decompiler.dispose();

        println("");
        println("REPORT_COMPLETE AnimIdDefinitionLookupReport");
    }
}
