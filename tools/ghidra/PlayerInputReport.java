// Reports the native player movement/action pipeline after control separation.
// The script is read-only and refuses to inspect an unexpected executable.
// @category SudekiMP

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;

import java.util.HashSet;
import java.util.Set;

public class PlayerInputReport extends GhidraScript {
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
            boolean decompileCallers,
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
            if (decompileCallers && caller != null) {
                decompile(decompiler, caller, "caller of " + role, seen);
            }
        }
    }

    @Override
    protected void run() throws Exception {
        String actualSha256 = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(actualSha256)) {
            throw new Exception("Unexpected executable SHA256: " + actualSha256);
        }

        println("SudekiMP player-input report");
        println("SHA256=" + actualSha256);
        println("ImageBase=" + currentProgram.getImageBase());

        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        Set<Address> seen = new HashSet<Address>();

        long[][] actionStrings = {
            {0x006c1ad4L, 0}, // ac_CharacterMoveFrwdBack
            {0x006c1af0L, 0}, // ac_CharacterMoveForwards
            {0x006c1b0cL, 0}, // ac_CharacterMoveBackwards
            {0x006c1b28L, 0}, // ac_CharacterMoveLeftRight
            {0x006c1b44L, 0}, // ac_CharacterMoveLeft
            {0x006c1b5cL, 0}, // ac_CharacterMoveRight
            {0x006c1b74L, 0}, // ac_CharacterTurnLeftRight
            {0x006c1b90L, 0}, // ac_CharacterMoveMod
            {0x006c1ba4L, 0}, // ac_CharacterAttackWeak
            {0x006c1bbcL, 0}, // ac_CharacterAttackStrong
            {0x006c1bd8L, 0}, // ac_CharacterAttackSweep
            {0x006c1bf0L, 0}, // ac_CharacterWeaponSelectNext
            {0x006c1c10L, 0}, // ac_CharacterWeaponSelectPrev
            {0x006c1c30L, 0}  // ac_CharacterBlock
        };
        String[] actionNames = {
            "ac_CharacterMoveFrwdBack",
            "ac_CharacterMoveForwards",
            "ac_CharacterMoveBackwards",
            "ac_CharacterMoveLeftRight",
            "ac_CharacterMoveLeft",
            "ac_CharacterMoveRight",
            "ac_CharacterTurnLeftRight",
            "ac_CharacterMoveMod",
            "ac_CharacterAttackWeak",
            "ac_CharacterAttackStrong",
            "ac_CharacterAttackSweep",
            "ac_CharacterWeaponSelectNext",
            "ac_CharacterWeaponSelectPrev",
            "ac_CharacterBlock"
        };
        for (int index = 0; index < actionStrings.length; ++index) {
            reportReferences(decompiler, actionStrings[index][0],
                actionNames[index], true, seen);
        }

        decompileAt(decompiler, 0x004277b0L,
            "controller input-event handler", seen);
        decompileAt(decompiler, 0x00427cf0L,
            "controller frame update", seen);
        decompileAt(decompiler, 0x00428b00L,
            "movement vector consumer", seen);
        decompileAt(decompiler, 0x004289d0L,
            "movement input preprocessing", seen);
        decompileAt(decompiler, 0x004291a0L,
            "movement vector camera transform", seen);
        decompileAt(decompiler, 0x00428640L,
            "controller mode predicate", seen);
        decompileAt(decompiler, 0x004286c0L,
            "button-state/action consumer", seen);
        decompileAt(decompiler, 0x004290d0L,
            "controller mode transition", seen);
        decompileAt(decompiler, 0x00429370L,
            "controller target assignment", seen);
        decompileAt(decompiler, 0x00429410L,
            "controller state transition helper", seen);
        decompileAt(decompiler, 0x00429ea0L,
            "first-person/controller helper", seen);
        decompileAt(decompiler, 0x004dae80L,
            "character arbiter movement submission", seen);
        decompileAt(decompiler, 0x004db070L,
            "CCharacterArbiter::SetSpeed export", seen);
        decompileAt(decompiler, 0x004030a0L,
            "CMovementController::SetAbsoluteDeltaMovement export", seen);

        reportReferences(decompiler, 0x00428b00L,
            "movement vector consumer", true, seen);
        reportReferences(decompiler, 0x004dae80L,
            "character arbiter movement submission", true, seen);
        reportReferences(decompiler, 0x004db070L,
            "CCharacterArbiter::SetSpeed", true, seen);
        reportReferences(decompiler, 0x004030a0L,
            "CMovementController::SetAbsoluteDeltaMovement", true, seen);

        println("");
        println("MATCHING CGamePadControlComponent SYMBOLS");
        SymbolIterator symbols = currentProgram.getSymbolTable().getAllSymbols(true);
        while (symbols.hasNext() && !monitor.isCancelled()) {
            Symbol symbol = symbols.next();
            String lower = symbol.getName(true).toLowerCase();
            if (!lower.contains("cgamepadcontrolcomponent")) {
                continue;
            }
            if (!(lower.contains("movement") || lower.contains("move") ||
                  lower.contains("controlfilter") || lower.contains("target") ||
                  lower.contains("firstperson"))) {
                continue;
            }
            println("  " + symbol.getAddress() + " " + symbol.getName(true) +
                " type=" + symbol.getSymbolType());
            Function function = currentProgram.getFunctionManager()
                .getFunctionAt(symbol.getAddress());
            decompile(decompiler, function,
                "matching CGamePadControlComponent symbol", seen);
        }

        decompiler.dispose();
    }
}
