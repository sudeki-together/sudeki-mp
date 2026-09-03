// Reports the exact native first-person fire and aim path needed by LAN input.
// Read-only: refuses to inspect any executable other than the supported build.
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

public class LanArenaRangedInputReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private Address address(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
    }

    private Function function(long value) {
        Function exact = currentProgram.getFunctionManager()
            .getFunctionAt(address(value));
        return exact != null ? exact : currentProgram.getFunctionManager()
            .getFunctionContaining(address(value));
    }

    private void decompile(DecompInterface decompiler, Function target,
            String role, Set<Address> seen) {
        if (target == null || !seen.add(target.getEntryPoint())) return;
        println("");
        println("===== " + role + " function=" + target.getEntryPoint() +
            " " + target.getName(true) + " =====");
        DecompileResults result = decompiler.decompileFunction(
            target, 120, monitor);
        println(result.decompileCompleted() ?
            result.getDecompiledFunction().getC() :
            "Decompiler failed: " + result.getErrorMessage());
    }

    private void inspect(DecompInterface decompiler, long value,
            String role, Set<Address> seen) {
        Function target = function(value);
        decompile(decompiler, target, role, seen);
        if (target == null) return;

        println("");
        println("CALLERS role=" + role + " function=" +
            target.getEntryPoint());
        ReferenceIterator callers = currentProgram.getReferenceManager()
            .getReferencesTo(target.getEntryPoint());
        while (callers.hasNext() && !monitor.isCancelled()) {
            Reference reference = callers.next();
            if (!reference.getReferenceType().isCall()) continue;
            Function owner = currentProgram.getFunctionManager()
                .getFunctionContaining(reference.getFromAddress());
            println("  callsite=" + reference.getFromAddress() + " caller=" +
                (owner == null ? "<none>" : owner.getEntryPoint() + " " +
                    owner.getName(true)));
            decompile(decompiler, owner, "caller of " + role, seen);
        }

        println("");
        println("DIRECT CALLS role=" + role + " function=" +
            target.getEntryPoint());
        InstructionIterator instructions = currentProgram.getListing()
            .getInstructions(target.getBody(), true);
        while (instructions.hasNext() && !monitor.isCancelled()) {
            Instruction instruction = instructions.next();
            for (Reference reference : instruction.getReferencesFrom()) {
                if (!reference.getReferenceType().isCall()) continue;
                Function callee = currentProgram.getFunctionManager()
                    .getFunctionAt(reference.getToAddress());
                println("  callsite=" + instruction.getAddress() + " callee=" +
                    (callee == null ? reference.getToAddress().toString() :
                        callee.getEntryPoint() + " " + callee.getName(true)));
                decompile(decompiler, callee,
                    "direct callee of " + role, seen);
            }
        }
    }

    @Override
    protected void run() throws Exception {
        String actual = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(actual)) {
            throw new Exception("Unexpected executable SHA256: " + actual);
        }
        println("SudekiMP LAN ranged-input report");
        println("SHA256=" + actual);
        println("ImageBase=" + currentProgram.getImageBase());

        long[] targets = {
            0x004286c0L, // controller combat-state consumer
            0x00534410L, // first-person held-weak fire routine
            0x004dba40L, // ranged firing eligibility
            0x004dc610L, // enter ranged firing presentation
            0x004dc720L, // leave ranged firing presentation
            0x004c6de0L, // missile selection
            0x004c7160L, // missile launch
            0x004c87b0L, // missile fire path
            0x004e85f0L, // native camera input event
            0x00428b00L, // controller camera/first-person aim update
            0x00429bc0L, // controller first-person policy
            0x00429ea0L, // first-person transition
            0x004dc330L, // arbiter first-person bit writer
            0x004dc530L, // first-person movement/aim submission
            0x004c9ae2L, // character ranged-state consumer A
            0x004c9b7eL, // character ranged-state consumer B
            0x004c9f69L, // character ranged-state consumer C
            0x004ca2b7L, // character ranged-state consumer D
            0x004ca3f3L, // character ranged-state consumer E
            0x004cad1aL  // character ranged-state consumer F
        };
        String[] roles = {
            "controller combat-state consumer",
            "first-person held-weak fire routine",
            "ranged firing eligibility",
            "enter ranged firing presentation",
            "leave ranged firing presentation",
            "missile selection",
            "missile launch",
            "missile fire path",
            "native camera input event",
            "controller camera/first-person aim update",
            "controller first-person policy",
            "first-person transition",
            "arbiter first-person bit writer",
            "first-person movement/aim submission",
            "character ranged-state consumer A",
            "character ranged-state consumer B",
            "character ranged-state consumer C",
            "character ranged-state consumer D",
            "character ranged-state consumer E",
            "character ranged-state consumer F"
        };

        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        Set<Address> seen = new HashSet<Address>();
        for (int index = 0; index < targets.length; ++index) {
            inspect(decompiler, targets[index], roles[index], seen);
        }
        decompiler.dispose();
    }
}
