// Reports native player-switching actions, group methods, and callers.
// The script is read-only and refuses to inspect an unexpected executable.
// @category SudekiMP

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.scalar.Scalar;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;

import java.util.HashSet;
import java.util.Set;

public class CharacterSwitchReport extends GhidraScript {
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

    private void decompileAt(
            DecompInterface decompiler,
            long value,
            String role,
            Set<Address> seen) {
        Address target = address(value);
        decompile(decompiler,
            currentProgram.getFunctionManager().getFunctionContaining(target),
            role + " target=" + target,
            seen);
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
        while (references.hasNext()) {
            Reference reference = references.next();
            Function caller = currentProgram.getFunctionManager().getFunctionContaining(
                reference.getFromAddress());
            println("  from=" + reference.getFromAddress() + " type=" +
                reference.getReferenceType() + " caller=" +
                (caller == null ? "<none>" : caller.getName(true)));
            if (caller != null && reference.getReferenceType().isCall()) {
                decompile(decompiler, caller, "caller of " + role, seen);
            }
        }
    }

    private void reportGlobalOffsetCandidates(
            DecompInterface decompiler,
            long globalValue,
            long[] offsets,
            String role,
            Set<Address> seen) {
        Address global = address(globalValue);
        ReferenceIterator references = currentProgram.getReferenceManager()
            .getReferencesTo(global);
        Set<Function> candidates = new HashSet<Function>();
        while (references.hasNext()) {
            Reference reference = references.next();
            Function function = currentProgram.getFunctionManager()
                .getFunctionContaining(reference.getFromAddress());
            if (function != null) {
                candidates.add(function);
            }
        }

        println("");
        println("GLOBAL/OFFSET CANDIDATES role=" + role + " global=" + global);
        for (Function function : candidates) {
            boolean matched = false;
            InstructionIterator instructions = currentProgram.getListing()
                .getInstructions(function.getBody(), true);
            while (instructions.hasNext()) {
                Instruction instruction = instructions.next();
                for (int operand = 0; operand < instruction.getNumOperands(); operand++) {
                    for (Object object : instruction.getOpObjects(operand)) {
                        if (!(object instanceof Scalar)) {
                            continue;
                        }
                        long value = ((Scalar)object).getUnsignedValue();
                        for (long offset : offsets) {
                            if (value == offset) {
                                println("  function=" + function.getEntryPoint() +
                                    " instruction=" + instruction);
                                matched = true;
                            }
                        }
                    }
                }
            }
            if (matched) {
                decompile(decompiler, function, role, seen);
            }
        }
    }

    @Override
    protected void run() throws Exception {
        String actualSha256 = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(actualSha256)) {
            throw new Exception("Unexpected executable SHA256: " + actualSha256);
        }

        println("SudekiMP character-switch report");
        println("SHA256=" + actualSha256);
        println("ImageBase=" + currentProgram.getImageBase());

        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        Set<Address> seen = new HashSet<Address>();

        reportReferences(decompiler, 0x006c1c44L, "ac_PrevChar string", seen);
        reportReferences(decompiler, 0x006c1c50L, "ac_NextChar string", seen);
        reportReferences(decompiler, 0x00424e10L,
            "CGroupPlayers::TogglePlayerSwitching", seen);
        reportReferences(decompiler, 0x00424e30L,
            "CGroupPlayers::GetPlayerSwitchingEnabled", seen);
        reportReferences(decompiler, 0x00424600L,
            "CGroupPlayers::PlayerIterGetNext", seen);
        reportReferences(decompiler, 0x004277b0L,
            "character input-event handler", seen);
        reportReferences(decompiler, 0x00427540L,
            "button-state predicate", seen);
        reportGlobalOffsetCandidates(decompiler, 0x00808da4L,
            new long[] { 0xf4L, 0xfcL },
            "controller functions touching Next/Prev states", seen);

        decompileAt(decompiler, 0x004277b0L, "main character input handler", seen);
        decompileAt(decompiler, 0x00427560L, "character controller state reset", seen);
        decompileAt(decompiler, 0x00427b70L, "adjacent controller function", seen);
        decompileAt(decompiler, 0x00427bf0L, "native QuickSkill helper", seen);
        decompileAt(decompiler, 0x00427cf0L,
            "character controller frame update", seen);
        decompileAt(decompiler, 0x00423f60L,
            "previous-character request consumer", seen);
        decompileAt(decompiler, 0x00424060L,
            "next-character request consumer", seen);
        decompileAt(decompiler, 0x00423ce0L,
            "previous-character party rotation", seen);
        decompileAt(decompiler, 0x00423b50L,
            "next-character party rotation", seen);
        decompileAt(decompiler, 0x004237b0L,
            "post-rotation control reassignment", seen);
        decompileAt(decompiler, 0x00423750L,
            "switch-target eligibility predicate", seen);
        decompileAt(decompiler, 0x00429370L,
            "controller-target pointer assignment helper", seen);
        decompileAt(decompiler, 0x004ef700L,
            "old/new AI-component transition", seen);
        decompileAt(decompiler, 0x004db520L,
            "old/new arbiter transition", seen);
        decompileAt(decompiler, 0x004b9680L,
            "old/new character component transition", seen);
        decompileAt(decompiler, 0x0042a370L,
            "post-switch camera/controller notification", seen);
        decompileAt(decompiler, 0x00424310L, "frequent CGroupPlayers helper", seen);
        decompileAt(decompiler, 0x00424cc0L, "CGroupPlayers candidate", seen);
        decompileAt(decompiler, 0x00424d30L, "CGroupPlayers candidate", seen);
        decompileAt(decompiler, 0x00424da0L, "CGroupPlayers candidate", seen);
        decompileAt(decompiler, 0x00424ef0L, "CGroupPlayers candidate", seen);
        decompileAt(decompiler, 0x004bb790L, "switch-toggle caller", seen);
        decompileAt(decompiler, 0x004c9d80L, "switch-toggle caller", seen);

        println("");
        println("MATCHING SYMBOLS");
        SymbolIterator symbols = currentProgram.getSymbolTable().getAllSymbols(true);
        while (symbols.hasNext() && !monitor.isCancelled()) {
            Symbol symbol = symbols.next();
            String lower = symbol.getName(true).toLowerCase();
            if (lower.contains("cgroupplayers") &&
                    (lower.contains("switch") || lower.contains("playeriter") ||
                     lower.contains("groupbyposition") || lower.contains("front"))) {
                println("  " + symbol.getAddress() + " " + symbol.getName(true) +
                    " type=" + symbol.getSymbolType());
                Function function = currentProgram.getFunctionManager()
                    .getFunctionAt(symbol.getAddress());
                decompile(decompiler, function,
                    "matching CGroupPlayers symbol", seen);
            }
        }

        decompiler.dispose();
    }
}
