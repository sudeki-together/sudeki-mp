// Confirms whether FUN_004e42f0 (0x4E42F0) is the ANIMID -> animation-definition
// resolver: does it take a semantic animation id (ANIMID_*, 0x00-0xC4), and how
// does it produce the definition pointer consumed by FUN_004e4f50 (0x4E4F50)?
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

public class AnimIdResolverConfirmReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private static final long RESOLVER = 0x004E42F0L;
    private static final long ANIM_PLAY = 0x004E4F50L;
    private static final long ANIMID_NAME_TABLE = 0x007363E8L;
    private static final int MAX_DECOMPILE_LINES = 110;

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
            println("  RESOLVER_CALLSITE in " + fn.getName() + " @0x" +
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
        println("SudekiMP ANIMID resolver confirmation report");
        println("SHA256=" + actualSha256);
        println("");

        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        println("=== PART A: the resolver itself ===");
        printDecompile(decompiler, RESOLVER, "RESOLVER");

        println("");
        println("=== PART B: resolver callee census ===");
        Function resolver = getFunctionAt(address(RESOLVER));
        if (resolver != null) {
            Set<Long> seen = new LinkedHashSet<Long>();
            InstructionIterator iterator = currentProgram.getListing()
                .getInstructions(resolver.getBody(), true);
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
                    boolean touchesAnimIds =
                        !callersOf(ANIMID_NAME_TABLE).isEmpty();
                    println(String.format(
                        "  RESOLVER_CALLEE 0x%08X %s",
                        target,
                        callee == null ? "<unresolved>" : callee.getName()));
                    if (callee != null && touchesAnimIds) {
                        // no-op: placeholder keeps output shape stable
                    }
                }
            }
            println("RESOLVER_CALLEE_COUNT=" + seen.size());
        }
        println("");

        println("=== PART C: resolver callsites (argument construction) ===");
        Set<Function> resolverCallers = callersOf(RESOLVER);
        println("RESOLVER_CALLER_COUNT=" + resolverCallers.size());
        for (Function fn : resolverCallers) {
            println(String.format(
                "RESOLVER_CALLER %s @0x%08X",
                fn.getName(), fn.getEntryPoint().getOffset()));
        }
        println("");
        for (Function fn : resolverCallers) {
            reportCallsites(fn, RESOLVER, 12);
        }

        println("=== PART D: does the resolver reference the ANIMID name table? ===");
        boolean resolverUsesAnimIdNames = false;
        Set<Function> nameUsers = callersOf(ANIMID_NAME_TABLE);
        for (Function fn : nameUsers) {
            if (fn.getEntryPoint().getOffset() == RESOLVER) {
                resolverUsesAnimIdNames = true;
            }
        }
        println("RESOLVER_REFERENCES_ANIMID_NAME_TABLE=" +
            (resolverUsesAnimIdNames ? "yes" : "no"));

        decompiler.dispose();
        println("");
        println("REPORT_COMPLETE AnimIdResolverConfirmReport");
    }
}
