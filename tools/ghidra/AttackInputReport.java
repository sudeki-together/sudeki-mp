// Reports Sudeki's native normal-attack input path for the local co-op proof.
// The script is read-only and refuses to inspect an unexpected executable.
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
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;

import java.util.HashSet;
import java.util.Set;

public class AttackInputReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private Address address(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
    }

    private void decompile(
            DecompInterface decompiler,
            Function function,
            String role,
            Set<Address> seen) {
        if (function == null || !seen.add(function.getEntryPoint())) {
            return;
        }
        DecompileResults result = decompiler.decompileFunction(function, 90, monitor);
        println("");
        println("===== " + role + " function=" + function.getEntryPoint() +
            " " + function.getName(true) + " =====");
        if (!result.decompileCompleted()) {
            println("Decompiler failed: " + result.getErrorMessage());
            return;
        }
        println(result.getDecompiledFunction().getC());
    }

    private Function functionAtOrContaining(long value) {
        Address target = address(value);
        Function function = currentProgram.getFunctionManager().getFunctionAt(target);
        if (function == null) {
            function = currentProgram.getFunctionManager().getFunctionContaining(target);
        }
        return function;
    }

    private void decompileAt(
            DecompInterface decompiler,
            long value,
            String role,
            Set<Address> seen) {
        decompile(decompiler, functionAtOrContaining(value),
            role + " target=" + address(value), seen);
    }

    private void reportReferences(
            DecompInterface decompiler,
            long value,
            String role,
            Set<Address> seen) {
        Address target = address(value);
        ReferenceIterator references = currentProgram.getReferenceManager()
            .getReferencesTo(target);
        println("");
        println("REFERENCES role=" + role + " target=" + target);
        while (references.hasNext() && !monitor.isCancelled()) {
            Reference reference = references.next();
            Function caller = currentProgram.getFunctionManager()
                .getFunctionContaining(reference.getFromAddress());
            println("  from=" + reference.getFromAddress() + " type=" +
                reference.getReferenceType() + " caller=" +
                (caller == null ? "<none>" : caller.getName(true)));
            decompile(decompiler, caller, "caller of " + role, seen);
        }
    }

    private void decompileDirectCalls(
            DecompInterface decompiler,
            long value,
            String role,
            Set<Address> seen) {
        Function function = functionAtOrContaining(value);
        if (function == null) {
            return;
        }
        InstructionIterator instructions = currentProgram.getListing()
            .getInstructions(function.getBody(), true);
        println("");
        println("DIRECT CALLS role=" + role + " function=" +
            function.getEntryPoint());
        while (instructions.hasNext() && !monitor.isCancelled()) {
            Instruction instruction = instructions.next();
            Reference[] references = instruction.getReferencesFrom();
            for (Reference reference : references) {
                if (!reference.getReferenceType().isCall()) {
                    continue;
                }
                Function callee = currentProgram.getFunctionManager()
                    .getFunctionAt(reference.getToAddress());
                println("  from=" + instruction.getAddress() + " to=" +
                    reference.getToAddress() + " callee=" +
                    (callee == null ? "<none>" : callee.getName(true)));
                decompile(decompiler, callee,
                    "direct callee of " + role, seen);
            }
        }
    }

    private void reportMatchingSymbols() {
        println("");
        println("MATCHING ATTACK/ARBITER SYMBOLS");
        SymbolIterator symbols = currentProgram.getSymbolTable().getAllSymbols(true);
        while (symbols.hasNext() && !monitor.isCancelled()) {
            Symbol symbol = symbols.next();
            String lower = symbol.getName(true).toLowerCase();
            if (!(lower.contains("attackweak") ||
                  lower.contains("attackstrong") ||
                  lower.contains("attacksweep") ||
                  lower.contains("characterblock") ||
                  (lower.contains("characterarbiter") &&
                   (lower.contains("attack") || lower.contains("input") ||
                    lower.contains("action") || lower.contains("block"))))) {
                continue;
            }
            println("  " + symbol.getAddress() + " " + symbol.getName(true) +
                " type=" + symbol.getSymbolType());
        }
    }

    @Override
    protected void run() throws Exception {
        String actualSha256 = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(actualSha256)) {
            throw new Exception("Unexpected executable SHA256: " + actualSha256);
        }

        println("SudekiMP normal-attack input report");
        println("SHA256=" + actualSha256);
        println("ImageBase=" + currentProgram.getImageBase());

        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        Set<Address> seen = new HashSet<Address>();

        decompileAt(decompiler, 0x004277b0L,
            "controller input-event handler", seen);
        decompileAt(decompiler, 0x004286c0L,
            "controller combat-state consumer", seen);
        decompileAt(decompiler, 0x004db0e0L,
            "five-state character-arbiter submission", seen);
        decompileAt(decompiler, 0x00401750L,
            "controller-target context helper", seen);
        decompileAt(decompiler, 0x004015e0L,
            "controller-target context cleanup", seen);

        reportReferences(decompiler, 0x004db0e0L,
            "five-state character-arbiter submission", seen);
        decompileDirectCalls(decompiler, 0x004db0e0L,
            "five-state character-arbiter submission", seen);
        reportMatchingSymbols();

        decompiler.dispose();
    }
}
