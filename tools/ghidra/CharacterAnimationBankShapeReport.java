// Reports the shape of Sudeki's character->animation binding: how a global
// ANIMID (semantic animation id, see AnimationIdNamesReport) becomes a per-
// character renderer selector, and whether that map is DATA or CODE.
//
// Read-only: refuses any executable other than the supported GOG build.
// @category SudekiMP

import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressIterator;
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

public class CharacterAnimationBankShapeReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private static final long ANIMID_NAME_TABLE = 0x007363e8L; // id -> char* name
    private static final long RENDERER_SELECTOR_SETTER = 0x00623000L;
    private static final long RENDERER_SELECTOR_GETTER = 0x006230b0L;
    private static final long RENDERER_SELECTOR_INVALIDATE = 0x00622f80L;
    private static final long ALL_SUBMODEL_SELECTOR_HELPER = 0x005e84f0L;

    private Address address(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
    }

    private Set<Function> functionsReferencing(long target) {
        Set<Function> result = new LinkedHashSet<Function>();
        ReferenceManager manager = currentProgram.getReferenceManager();
        ReferenceIterator iterator = manager.getReferencesTo(address(target));
        while (iterator.hasNext()) {
            Reference reference = iterator.next();
            Function owner = getFunctionContaining(reference.getFromAddress());
            if (owner != null) {
                result.add(owner);
            }
        }
        return result;
    }

    /** Walk the body of fn and print any callsite of target with the scalar
     *  operands of the preceding instructions, so literal selector ids show. */
    private void reportCallsites(
        Function fn, long target, String label, int lookback, int maxSites
    ) {
        List<String> window = new ArrayList<String>();
        int sites = 0;
        InstructionIterator iterator =
            currentProgram.getListing().getInstructions(fn.getBody(), true);
        while (iterator.hasNext() && sites < maxSites) {
            Instruction instruction = iterator.next();
            window.add(String.format(
                "0x%08X %s %s",
                instruction.getAddress().getOffset(),
                instruction.getMnemonicString(),
                instruction.toString()));
            if (window.size() > lookback + 1) {
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
            ++sites;
            println("  CALLSITE " + label + " in " +
                fn.getName() + " @0x" +
                Integer.toHexString((int)fn.getEntryPoint().getOffset()));
            for (int index = 0; index < window.size(); ++index) {
                println("    " + window.get(index));
            }
            println("");
        }
        println("CALLSITE_COUNT " + label + " fn=" + fn.getName() +
            " n=" + sites);
    }

    private void dumpDwords(String label, long start, int count) {
        println("--- " + label + " @0x" + Long.toHexString(start) +
            " n=" + count);
        StringBuilder line = new StringBuilder();
        for (int index = 0; index < count; ++index) {
            long at = start + (long)index * 4L;
            int value;
            try {
                value = currentProgram.getMemory().getInt(address(at));
            } catch (Exception error) {
                println("  <unreadable at 0x" + Long.toHexString(at) + ">");
                break;
            }
            line.append(String.format("%08X ", value));
            if (line.length() >= 96) {
                println("  " + line.toString());
                line.setLength(0);
            }
        }
        if (line.length() > 0) {
            println("  " + line.toString());
        }
    }

    /** Scan a function body for instructions that imply a selector-sized
     *  scalar, which surfaces the literal ids the game pushes per branch. */
    private void reportSelectorLiterals(Function fn, int maxPrint) {
        InstructionIterator iterator =
            currentProgram.getListing().getInstructions(fn.getBody(), true);
        int printed = 0;
        while (iterator.hasNext() && printed < maxPrint) {
            Instruction instruction = iterator.next();
            int operands = instruction.getNumOperands();
            for (int index = 0; index < operands; ++index) {
                Object[] objects = instruction.getOpObjects(index);
                for (int inner = 0; inner < objects.length; ++inner) {
                    if (!(objects[inner] instanceof ghidra.program.model.scalar.Scalar)) {
                        continue;
                    }
                    long value =
                        ((ghidra.program.model.scalar.Scalar)objects[inner])
                            .getUnsignedValue();
                    if (value < 1L || value > 0x200L) {
                        continue;
                    }
                    println(String.format(
                        "  LITERAL 0x%08X %s -> %d",
                        instruction.getAddress().getOffset(),
                        instruction.toString(), value));
                    ++printed;
                    break;
                }
                if (printed >= maxPrint) {
                    break;
                }
            }
        }
        if (printed >= maxPrint) {
            println("  LITERAL_TRUNCATED at " + maxPrint);
        }
    }

    @Override
    protected void run() throws Exception {
        String actualSha256 = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(actualSha256)) {
            throw new Exception("Unexpected executable SHA256: " + actualSha256);
        }
        println("SudekiMP character/animation binding-shape report");
        println("SHA256=" + actualSha256);
        println("EXE=" + currentProgram.getExecutablePath());
        println("");

        // Part A: who reads the global ANIMID name table?
        println("=== PART A: references to the ANIMID name table ===");
        Set<Function> nameTableUsers = functionsReferencing(ANIMID_NAME_TABLE);
        for (Function fn : nameTableUsers) {
            println(String.format(
                "NAME_TABLE_USER %s @0x%08X",
                fn.getName(), fn.getEntryPoint().getOffset()));
        }
        println("NAME_TABLE_USER_COUNT=" + nameTableUsers.size());
        println("");

        // Part B: decompile the first few users so the ANIMID usage is visible.
        println("=== PART B: decompiled ANIMID consumers ===");
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        int decompiled = 0;
        for (Function fn : nameTableUsers) {
            if (decompiled >= 4) {
                println("DECOMPILE_TRUNCATED after " + decompiled);
                break;
            }
            try {
                DecompileResults results =
                    decompiler.decompileFunction(fn, 60, monitor);
                if (results == null || !results.decompileCompleted()) {
                    println("decompile_failed " + fn.getName());
                    continue;
                }
                String text = results.getDecompiledFunction().getC();
                String[] lines = text.split("\n");
                println("--- " + fn.getName() + " @0x" +
                    Long.toHexString(fn.getEntryPoint().getOffset()) +
                    " lines=" + lines.length);
                for (int index = 0; index < lines.length && index < 40; ++index) {
                    println("  " + lines[index]);
                }
                ++decompiled;
            } catch (Exception error) {
                println("decompile_failed " + fn.getName() + " " + error);
            }
        }
        decompiler.dispose();
        println("");

        // Part C: literal selectors pushed at the renderer setter callsites.
        println("=== PART C: renderer selector setter callsites ===");
        Set<Function> setterCallers = functionsReferencing(RENDERER_SELECTOR_SETTER);
        for (Function fn : setterCallers) {
            println(String.format(
                "SELECTOR_SETTER_CALLER %s @0x%08X",
                fn.getName(), fn.getEntryPoint().getOffset()));
        }
        println("SELECTOR_SETTER_CALLER_COUNT=" + setterCallers.size());
        println("");
        for (Function fn : setterCallers) {
            reportCallsites(fn, RENDERER_SELECTOR_SETTER,
                "RENDERER_SELECTOR_SETTER", 6, 2);
            println("");
        }

        // Part D: literal ids visible inside a couple of those functions.
        println("=== PART D: selector-sized literals in setter callers ===");
        int scanned = 0;
        for (Function fn : setterCallers) {
            if (scanned >= 3) {
                println("LITERAL_SCAN_TRUNCATED after " + scanned + " functions");
                break;
            }
            println("--- literals in " + fn.getName());
            reportSelectorLiterals(fn, 60);
            ++scanned;
        }
        println("");

        println("=== PART E: neighbours of the ANIMID name table ===");
        dumpDwords("before", ANIMID_NAME_TABLE - 32L * 4L, 32);
        println("");
        dumpDwords("after", ANIMID_NAME_TABLE + 193L * 4L, 64);
        println("");

        println("=== PART F: other animation entry points ===");
        long[] others = new long[] {
            RENDERER_SELECTOR_GETTER, RENDERER_SELECTOR_INVALIDATE,
            ALL_SUBMODEL_SELECTOR_HELPER
        };
        String[] labels = new String[] {
            "RENDERER_SELECTOR_GETTER", "RENDERER_SELECTOR_INVALIDATE",
            "ALL_SUBMODEL_SELECTOR_HELPER"
        };
        for (int index = 0; index < others.length; ++index) {
            Set<Function> users = functionsReferencing(others[index]);
            println(labels[index] + "_USERS=" + users.size());
            for (Function fn : users) {
                println(String.format(
                    "  %s USER %s @0x%08X",
                    labels[index], fn.getName(),
                    fn.getEntryPoint().getOffset()));
            }
        }
        println("");
        println("REPORT_COMPLETE CharacterAnimationBankShapeReport");
    }
}
