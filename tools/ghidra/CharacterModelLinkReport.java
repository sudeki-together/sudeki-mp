// CharacterModelLinkReport — find the character -> CNewGameModelAnimation link
// offset for BOTH melee (Tal/Buki) and ranged (Ailish/Elco) heroes, so the host
// ANIMID read can reach the correct model. The ranged offset is known to be
// +0x134 (AILISH_RANGED_COMPONENT_OFFSET); this report finds the melee offset.
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

public class CharacterModelLinkReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    // TsaGetCurrentAnimation: method on the model (this in ECX), reads
    // [[this+0xF8]+2] guarded by this[0x131]==3.
    private static final long GET_CURRENT_ANIM = 0x0043AF10L;
    // FUN_00588110: the model (CNewGameModelAnimation) constructor.
    private static final long MODEL_CTOR = 0x00588110L;

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
            DecompileResults results = decompiler.decompileFunction(fn, 90, monitor);
            if (results == null || !results.decompileCompleted()) {
                println("decompile_failed " + fn.getName());
                return;
            }
            println("SIGNATURE " +
                results.getDecompiledFunction().getSignature());
            String[] lines = results.getDecompiledFunction().getC().split("\n");
            for (int index = 0; index < lines.length; ++index) {
                if (index >= 120) {
                    println("  ...TRUNCATED (" + lines.length + " lines total)");
                    break;
                }
                println("  " + lines[index]);
            }
        } catch (Exception error) {
            println("decompile_failed " + fn.getName() + " " + error);
        }
    }

    private void reportCallsites(Function fn, long target, int lookback) {
        List<String> window = new ArrayList<String>();
        InstructionIterator iterator =
            currentProgram.getListing().getInstructions(fn.getBody(), true);
        while (iterator.hasNext()) {
            Instruction instruction = iterator.next();
            window.add(String.format(
                "0x%08X %s",
                instruction.getAddress().getOffset(),
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
            println("  CALLSITE in " + fn.getName() + " @0x" +
                Long.toHexString(fn.getEntryPoint().getOffset()));
            for (int index = 0; index < window.size(); ++index) {
                println("    " + window.get(index));
            }
            println("");
        }
    }

    @Override
    protected void run() throws Exception {
        String actualSha256 = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(actualSha256)) {
            throw new Exception("Unexpected executable SHA256: " + actualSha256);
        }
        println("SudekiMP character -> model link report");
        println("SHA256=" + actualSha256);
        println("");

        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        // PART A: direct callers of TsaGetCurrentAnimation + callsite ASM.
        println("=== PART A: TsaGetCurrentAnimation callers (how is `this` loaded?) ===");
        Set<Function> animCallers = callersOf(GET_CURRENT_ANIM);
        println("GET_CURRENT_ANIM_CALLER_COUNT=" + animCallers.size());
        for (Function fn : animCallers) {
            println(String.format(
                "GET_CURRENT_ANIM_CALLER %s @0x%08X",
                fn.getName(), fn.getEntryPoint().getOffset()));
        }
        for (Function fn : animCallers) {
            reportCallsites(fn, GET_CURRENT_ANIM, 16);
            printDecompile(decompiler, fn.getEntryPoint().getOffset(),
                "GET_CURRENT_ANIM_CALLER");
        }

        // PART B: data references to TsaGetCurrentAnimation (vtable dispatch check).
        println("");
        println("=== PART B: data references to TsaGetCurrentAnimation (vtable?) ===");
        {
            ReferenceManager manager = currentProgram.getReferenceManager();
            ReferenceIterator iterator = manager.getReferencesTo(address(GET_CURRENT_ANIM));
            int dataCount = 0;
            while (iterator.hasNext()) {
                Reference reference = iterator.next();
                if (reference.getReferenceType().isCall()) {
                    continue;
                }
                println(String.format(
                    "DATAREF from 0x%08X to TsaGetCurrentAnimation",
                    reference.getFromAddress().getOffset()));
                ++dataCount;
            }
            println("GET_CURRENT_ANIM_DATAREF_COUNT=" + dataCount);
        }

        // PART C: model constructor + its callers (where the model is built and
        // linked to the character).
        println("");
        println("=== PART C: model constructor FUN_00588110 + callers ===");
        printDecompile(decompiler, MODEL_CTOR, "MODEL_CTOR");
        Set<Function> ctorCallers = callersOf(MODEL_CTOR);
        println("MODEL_CTOR_CALLER_COUNT=" + ctorCallers.size());
        for (Function fn : ctorCallers) {
            println(String.format(
                "MODEL_CTOR_CALLER %s @0x%08X",
                fn.getName(), fn.getEntryPoint().getOffset()));
            printDecompile(decompiler, fn.getEntryPoint().getOffset(),
                "MODEL_CTOR_CALLER");
        }

        decompiler.dispose();
        println("");
        println("REPORT_COMPLETE CharacterModelLinkReport");
    }
}
