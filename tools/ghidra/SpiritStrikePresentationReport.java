// Reports exact-build Spirit Strike presentation subevents and audio/effect calls.
// The script is read-only and refuses to inspect an unexpected executable.
// @category SudekiMP

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

import java.util.HashSet;
import java.util.Set;

public class SpiritStrikePresentationReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private Address address(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
    }

    private Function function(long value) {
        Function exact = currentProgram.getFunctionManager().getFunctionAt(address(value));
        return exact != null ? exact :
            currentProgram.getFunctionManager().getFunctionContaining(address(value));
    }

    private void dump(
            DecompInterface decompiler, long value, String role,
            Set<Address> decompiled) {
        Function target = function(value);
        println("");
        println("===== " + role + " target=" + address(value) + " function=" +
            (target == null ? "<none>" : target.getEntryPoint() + " " +
            target.getName(true)) + " =====");
        if (target == null || !decompiled.add(target.getEntryPoint())) {
            return;
        }
        DecompileResults result = decompiler.decompileFunction(target, 180, monitor);
        println(result.decompileCompleted() ? result.getDecompiledFunction().getC() :
            "Decompiler failed: " + result.getErrorMessage());
        println("DIRECT CALLS:");
        for (Instruction instruction = currentProgram.getListing().getInstructionAt(
                target.getBody().getMinAddress());
                instruction != null && target.getBody().contains(instruction.getAddress());
                instruction = instruction.getNext()) {
            for (Reference reference : instruction.getReferencesFrom()) {
                if (!reference.getReferenceType().isCall()) {
                    continue;
                }
                Function callee = currentProgram.getFunctionManager()
                    .getFunctionAt(reference.getToAddress());
                println("  " + instruction.getAddress() + " -> " +
                    reference.getToAddress() + " " +
                    (callee == null ? "<none>" : callee.getName(true)));
            }
        }
    }

    private void callers(long value, String role) {
        Function target = function(value);
        println("");
        println("CALLERS " + role + " target=" +
            (target == null ? address(value) : target.getEntryPoint()));
        if (target == null) {
            return;
        }
        ReferenceIterator references = currentProgram.getReferenceManager()
            .getReferencesTo(target.getEntryPoint());
        while (references.hasNext()) {
            Reference reference = references.next();
            if (!reference.getReferenceType().isCall()) {
                continue;
            }
            Function caller = currentProgram.getFunctionManager()
                .getFunctionContaining(reference.getFromAddress());
            println("  " + reference.getFromAddress() + " " +
                (caller == null ? "<none>" : caller.getEntryPoint() + " " +
                caller.getName(true)));
        }
    }

    private void reportFunctionsInRange(
            DecompInterface decompiler, long first, long last,
            Set<Address> decompiled) {
        println("");
        println("===== FUNCTIONS " + address(first) + ".." + address(last) + " =====");
        FunctionIterator functions = currentProgram.getFunctionManager()
            .getFunctions(address(first), true);
        while (functions.hasNext()) {
            Function function = functions.next();
            long entry = function.getEntryPoint().getOffset();
            if (entry > last) {
                break;
            }
            dump(decompiler, entry, "Manager-range function", decompiled);
        }
    }

    private void reportPointerTable(long value, int count, String role)
            throws Exception {
        Memory memory = currentProgram.getMemory();
        println("");
        println("===== POINTER TABLE " + role + " at=" + address(value) + " =====");
        for (int index = 0; index < count; ++index) {
            Address slot = address(value + index * 4L);
            long pointer = Integer.toUnsignedLong(memory.getInt(slot));
            Function target = currentProgram.getFunctionManager()
                .getFunctionAt(address(pointer));
            println("  [" + index + "] slot=" + slot + " value=" +
                address(pointer) + " " +
                (target == null ? "<none>" : target.getName(true)));
        }
    }

    @Override
    protected void run() throws Exception {
        String actualSha256 = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(actualSha256)) {
            throw new Exception("Unexpected executable SHA256: " + actualSha256);
        }
        println("SudekiMP Spirit Strike presentation report");
        println("SHA256=" + actualSha256);
        println("ImageBase=" + currentProgram.getImageBase());

        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        Set<Address> decompiled = new HashSet<Address>();
        long[] values = {
            0x00410570L, 0x00410900L, 0x00410920L, 0x00410c20L,
            0x00410e70L, 0x00410ee0L, 0x004cc080L, 0x004cc150L,
            0x0052b0b0L, 0x0052b2a0L, 0x0052b2e0L, 0x0052b430L,
            0x0052b590L, 0x0052b630L, 0x0052b6b0L, 0x00417da0L,
            0x00418de0L, 0x00418e30L, 0x00418e70L, 0x00417090L,
            0x0068aeb0L, 0x005061d0L, 0x0052b060L, 0x0040eed0L,
            0x00418a80L, 0x00404e20L, 0x004170b0L
        };
        String[] roles = {
            "FireSoul", "IsSoulActive", "SoulReachedTarget", "Strike stage start",
            "StartStrikeRumble", "StrikeCompleted", "StartSpiritStrikeAttack",
            "StartSpiritStrikeAttack worker", "Soul launch", "Soul time edge",
            "SpiritStrikeData serializer", "CSpiritStrike resource ctor",
            "CSpiritStrike resource load", "PowerUp resource load",
            "Attack resource load", "Native spawned effect/entity creation",
            "CSFX PlaySfx", "CSFX PlaySfxWithHandle", "CSFX PlaySFXWithAll",
            "CSound PlayCue", "Audio cue submit", "Soul scalar setter",
            "Soul launch initializer", "Soul runtime constructor",
            "Effect/entity resource spawn", "Character sound cue playback",
            "GetSound singleton"
        };
        for (int index = 0; index < values.length; ++index) {
            dump(decompiler, values[index], roles[index], decompiled);
        }
        reportFunctionsInRange(decompiler, 0x0040f000L, 0x004113ffL, decompiled);
        reportPointerTable(0x006ca30cL, 8, "CSpiritStrikeManager primary vftable");
        reportPointerTable(0x006ca330L, 2, "CSpiritStrikeManager secondary vftable");
        reportPointerTable(0x006ca338L, 8, "CSpiritStrikeManager singleton vftable");
        callers(0x00410570L, "FireSoul");
        callers(0x00410900L, "IsSoulActive");
        callers(0x00410920L, "SoulReachedTarget");
        callers(0x00410c20L, "Strike stage start");
        callers(0x00410e70L, "StartStrikeRumble");
        callers(0x00410ee0L, "StrikeCompleted");
        callers(0x0052b0b0L, "Soul launch");
        callers(0x0052b2a0L, "Soul time edge");
        callers(0x00417da0L, "Native spawned effect/entity creation");
        callers(0x00418de0L, "CSFX PlaySfx");
        callers(0x00418e30L, "CSFX PlaySfxWithHandle");
        callers(0x00418e70L, "CSFX PlaySFXWithAll");
        callers(0x00417090L, "CSound PlayCue");
        callers(0x0068aeb0L, "Audio cue submit");
        callers(0x00404e20L, "Character sound cue playback");
        callers(0x004cc080L, "StartSpiritStrikeAttack");
        decompiler.dispose();
    }
}
