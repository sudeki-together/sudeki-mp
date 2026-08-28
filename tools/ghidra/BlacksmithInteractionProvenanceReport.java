// Traces the exact-build world-interaction message that precedes scripted
// blacksmith inventory population and UIBlackSmithStart. Read-only and
// hash-gated; it never modifies the program or project.
// @category SudekiMP

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

import java.util.HashSet;
import java.util.Set;

public class BlacksmithInteractionProvenanceReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private Address address(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
    }

    private void decompile(DecompInterface decompiler, Function function,
            String role, Set<Address> seen) {
        if (function == null || !seen.add(function.getEntryPoint())) {
            return;
        }
        DecompileResults result = decompiler.decompileFunction(
            function, 120, monitor);
        println("\n===== " + role + " function=" +
            function.getEntryPoint() + " " + function.getName(true) +
            " =====");
        println(result.decompileCompleted() ?
            result.getDecompiledFunction().getC() :
            "Decompiler failed: " + result.getErrorMessage());
    }

    private void decompileAt(DecompInterface decompiler, long value,
            String role, Set<Address> seen) {
        decompile(decompiler,
            currentProgram.getFunctionManager().getFunctionContaining(
                address(value)), role, seen);
    }

    private void callers(DecompInterface decompiler, long value,
            String role, Set<Address> seen) {
        Address target = address(value);
        ReferenceIterator references = currentProgram.getReferenceManager()
            .getReferencesTo(target);
        println("\nREFERENCES role=" + role + " target=" + target);
        while (references.hasNext() && !monitor.isCancelled()) {
            Reference reference = references.next();
            Function caller = currentProgram.getFunctionManager()
                .getFunctionContaining(reference.getFromAddress());
            println("REFERENCE from=" + reference.getFromAddress() +
                " type=" + reference.getReferenceType() + " caller=" +
                (caller == null ? "<none>" :
                    caller.getEntryPoint() + " " + caller.getName(true)));
            if (reference.getReferenceType().isCall() && caller != null) {
                decompile(decompiler, caller, "caller of " + role, seen);
            }
        }
    }

    @Override
    protected void run() throws Exception {
        String actual = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(actual)) {
            throw new Exception("Unexpected executable SHA256: " + actual);
        }
        println("SudekiMP blacksmith interaction provenance report");
        println("SHA256=" + actual);
        println("ImageBase=" + currentProgram.getImageBase());

        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        Set<Address> seen = new HashSet<Address>();

        long[] targets = {
            0x0040ccd0L, // construct/enqueue interaction SerialMessage
            0x0040d6f0L, // dispatch current source actor into action candidates
            0x0040d7a0L, // select OnAction candidate and enqueue it
            0x0040d9a0L, // validate an OnAction candidate
            0x0040cfd0L, // adjacent trigger scan / candidate population
            0x0040d320L, // adjacent candidate lifecycle
            0x0040d530L, // adjacent candidate lifecycle
            0x004b5c30L, // CUsable collision/source callback
            0x004b5ce0L, // CUsable interaction eligibility
            0x004b5fa0L, // CUsable CanUse predicate
            0x004b6c70L, // CUsable current-target assignment
            0x005c37b0L, // script-message queue insertion helper
            0x005c38d0L, // construct/schedule the returned SOL task
            0x005c41d0L, // SOL bytecode interpreter step
            0x005c4970L, // opcode 0x27 compiled/global binding dispatch
            0x005b9ef0L, // intrusive/shared container append helper
            0x00583bc0L, // known script/native dispatch candidate
            0x00584050L, // adjacent script/native dispatch candidate
            0x00526df0L, // scripted object's OnAction serialization
            0x00527110L, // ResourceName construction for OnAction
            0x00492c40L, // UIBlackSmithStart
            0x004b10f0L, // BlackSmithClearInventory
            0x004b1110L, // BlackSmithAddItem
            0x00492c70L, // BlackSmithSetName(text)
            0x00492cc0L  // BlackSmithSetName(id)
        };
        String[] roles = {
            "interaction message enqueue", "action source dispatch",
            "OnAction candidate dispatch", "OnAction candidate validator",
            "trigger candidate scan", "candidate lifecycle A",
            "candidate lifecycle B", "usable collision callback",
            "usable eligibility", "usable CanUse", "usable target assignment",
            "script message submission",
            "SOL task construction", "SOL bytecode interpreter step",
            "opcode 0x27 global binding dispatch",
            "message queue append", "script/native dispatch A",
            "script/native dispatch B", "scripted object serialization",
            "ResourceName constructor", "UIBlackSmithStart",
            "BlackSmithClearInventory", "BlackSmithAddItem",
            "BlackSmithSetName text", "BlackSmithSetName id"
        };
        for (int index = 0; index < targets.length; ++index) {
            decompileAt(decompiler, targets[index], roles[index], seen);
        }
        for (int index = 0; index < targets.length; ++index) {
            callers(decompiler, targets[index], roles[index], seen);
        }
        decompiler.dispose();
    }
}
