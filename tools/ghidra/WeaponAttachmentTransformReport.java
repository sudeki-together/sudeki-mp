// Exact-build, read-only trace of Sudeki's character-weapon attachment path.
// @category SudekiMP

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

import java.util.HashSet;
import java.util.Set;

public class WeaponAttachmentTransformReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private Address address(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
    }

    private Function function(long value) {
        Function result = currentProgram.getFunctionManager()
            .getFunctionAt(address(value));
        return result != null ? result : currentProgram.getFunctionManager()
            .getFunctionContaining(address(value));
    }

    private void decompile(DecompInterface decompiler, long value,
            String role, Set<Address> seen) {
        Function target = function(value);
        if (target == null) {
            println("MISSING role=" + role + " target=" + address(value));
            return;
        }
        if (!seen.add(target.getEntryPoint())) {
            println("SEEN role=" + role + " target=" + address(value) +
                " function=" + target.getEntryPoint());
            return;
        }
        DecompileResults result = decompiler.decompileFunction(
            target, 120, monitor);
        println("");
        println("===== " + role + " target=" + address(value) +
            " function=" + target.getEntryPoint() + " " +
            target.getName(true) + " =====");
        println(result.decompileCompleted() ?
            result.getDecompiledFunction().getC() :
            "Decompiler failed: " + result.getErrorMessage());
    }

    private void callers(DecompInterface decompiler, long value,
            String role, Set<Address> seen) {
        Function target = function(value);
        if (target == null) {
            return;
        }
        println("");
        println("CALLERS role=" + role + " function=" +
            target.getEntryPoint());
        ReferenceIterator references = currentProgram.getReferenceManager()
            .getReferencesTo(target.getEntryPoint());
        while (references.hasNext() && !monitor.isCancelled()) {
            Reference reference = references.next();
            if (!reference.getReferenceType().isCall()) {
                continue;
            }
            Function caller = currentProgram.getFunctionManager()
                .getFunctionContaining(reference.getFromAddress());
            println("  callsite=" + reference.getFromAddress() +
                " caller=" + (caller == null ? "<none>" :
                    caller.getEntryPoint() + " " + caller.getName(true)));
            if (caller != null) {
                decompile(decompiler, caller.getEntryPoint().getOffset(),
                    "caller of " + role, seen);
            }
        }
    }

    private void instructions(long value, String role) {
        Function target = function(value);
        if (target == null) {
            return;
        }
        println("");
        println("INSTRUCTIONS role=" + role + " function=" +
            target.getEntryPoint());
        InstructionIterator iterator = currentProgram.getListing()
            .getInstructions(target.getBody(), true);
        while (iterator.hasNext() && !monitor.isCancelled()) {
            Instruction instruction = iterator.next();
            println("  " + instruction.getAddress() + " " + instruction);
        }
    }

    @Override
    protected void run() throws Exception {
        String actualSha256 = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(actualSha256)) {
            throw new Exception("Unexpected executable SHA256: " + actualSha256);
        }
        println("SudekiMP weapon attachment transform report");
        println("SHA256=" + actualSha256);
        println("ImageBase=" + currentProgram.getImageBase());

        long[] targets = {
            0x004d8630L, // weapon model -> character bone attachment
            0x005e3740L, // weapon locator ResourceName helper
            0x00511ae0L, // attached locator matrix lookup
            0x00511cc0L, // position world matrix getter
            0x00511960L, // parent-position attachment install
            0x00511a50L, // attachment locator/index install
            0x00511d00L, // attachment link helper
            0x00511da0L, // attachment unlink helper
            0x00510a40L, // position pre-attachment update
            0x00510f90L, // position post-attachment update
            0x004d80b0L, // first weapon presentation setup
            0x004d8470L, // alternate weapon presentation setup
            0x004d92d0L, // weapon presentation mode
            0x004d8be0L, // weapon refresh helper
            0x004d8fb0L, // weapon refresh helper
            0x00588a90L, // first/world character model switch
            0x00511b30L  // character model attachment switch
        };
        String[] roles = {
            "weapon bone attachment installer",
            "weapon locator ResourceName helper",
            "attached locator matrix lookup",
            "position world matrix getter",
            "parent-position attachment install",
            "attachment locator/index install",
            "attachment link helper",
            "attachment unlink helper",
            "position pre-attachment update",
            "position post-attachment update",
            "first weapon presentation setup",
            "alternate weapon presentation setup",
            "weapon presentation mode",
            "weapon refresh helper A",
            "weapon refresh helper B",
            "first/world character model switch",
            "character model attachment switch"
        };

        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        Set<Address> seen = new HashSet<Address>();
        for (int index = 0; index < targets.length; ++index) {
            decompile(decompiler, targets[index], roles[index], seen);
            callers(decompiler, targets[index], roles[index], seen);
        }
        instructions(0x004d8630L, "weapon bone attachment installer");
        instructions(0x005e3740L, "weapon locator ResourceName helper");
        instructions(0x00511a50L, "attachment locator/index install");
        instructions(0x004d80b0L, "first weapon presentation setup");
        instructions(0x004d8470L, "alternate weapon presentation setup");
        instructions(0x004d8280L, "first-person weapon reattachment");
        instructions(0x004d8300L, "alternate first-person weapon reattachment");
        instructions(0x005888f0L, "ranged presentation owner");
        instructions(0x00588ff0L, "temporary model presentation owner");
        decompiler.dispose();
    }
}
