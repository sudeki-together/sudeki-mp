// Reports static anchors for the first Phase 5 Skill Strike trace.
// The script is read-only and refuses to inspect an unexpected executable.
// @category SudekiMP

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;

public class PlasmaticaPhase5Report extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private Address address(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(value);
    }

    private void reportInstruction(long value, String role) throws Exception {
        Address location = address(value);
        Instruction instruction = currentProgram.getListing().getInstructionAt(location);
        if (instruction == null) {
            throw new Exception("No instruction at " + location + " (" + role + ")");
        }
        Function function = currentProgram.getFunctionManager().getFunctionContaining(location);
        println(String.format("%-32s %s  %-48s function=%s",
            role, location, instruction, function == null ? "<none>" : function.getName()));
    }

    private void reportReferences(long value, String role) {
        Address location = address(value);
        ReferenceIterator references = currentProgram.getReferenceManager().getReferencesTo(location);
        int count = 0;
        StringBuilder from = new StringBuilder();
        while (references.hasNext()) {
            Reference reference = references.next();
            if (count < 40) {
                if (from.length() != 0) {
                    from.append(", ");
                }
                from.append(reference.getFromAddress());
            }
            count++;
        }
        println(String.format("%-32s %s references=%d from=[%s]", role, location, count, from));
    }

    private void reportSymbols(String needle) {
        println("Symbols containing '" + needle + "':");
        SymbolIterator symbols = currentProgram.getSymbolTable().getAllSymbols(true);
        int count = 0;
        while (symbols.hasNext()) {
            Symbol symbol = symbols.next();
            if (symbol.getName(true).toLowerCase().contains(needle.toLowerCase())) {
                println("  " + symbol.getAddress() + "  " + symbol.getName(true));
                count++;
            }
        }
        println("  count=" + count);
    }

    private void reportFunctions(long start, long end) {
        println(String.format("Functions in [%08X, %08X]:", start, end));
        FunctionIterator functions = currentProgram.getFunctionManager().getFunctions(address(start), true);
        while (functions.hasNext()) {
            Function function = functions.next();
            long entry = function.getEntryPoint().getOffset();
            if (entry > end) {
                break;
            }
            println(String.format("  %s-%s  %s", function.getEntryPoint(),
                function.getBody().getMaxAddress(), function.getName(true)));
        }
    }

    private void decompile(DecompInterface decompiler, long value, String role) {
        Function function = currentProgram.getFunctionManager().getFunctionAt(address(value));
        if (function == null) {
            println(role + ": no function at " + address(value));
            return;
        }
        DecompileResults result = decompiler.decompileFunction(function, 60, monitor);
        println("");
        println("===== " + role + " " + function.getEntryPoint() + " " + function.getName(true) + " =====");
        if (!result.decompileCompleted()) {
            println("Decompiler failed: " + result.getErrorMessage());
            return;
        }
        println(result.getDecompiledFunction().getC());
    }

    @Override
    protected void run() throws Exception {
        String actualSha256 = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(actualSha256)) {
            throw new Exception("Unexpected executable SHA256: " + actualSha256);
        }

        println("SudekiMP Plasmatica Phase 5 static report");
        println("SHA256=" + actualSha256);
        println("ImageBase=" + currentProgram.getImageBase());
        println("");

        reportInstruction(0x004998a1L, "Quick Menu call CSkill::Use");
        reportInstruction(0x004b4810L, "CSkill::Use entry");
        reportInstruction(0x004b488eL, "Load selected skill pointer");
        reportInstruction(0x004b48b6L, "Subtract skill resource cost");
        reportInstruction(0x004b4b4cL, "Set queued/active byte");
        reportInstruction(0x004b4b50L, "Store selected slot");
        reportInstruction(0x004b4b5dL, "Begin generic skill protection");
        reportInstruction(0x004b47cdL, "End skill: decrement invulnerability refcount");
        reportInstruction(0x004b47e6L, "End skill: clear IsUsingSkill flag");
        reportReferences(0x004b4810L, "CSkill::Use xrefs");
        reportReferences(0x004b50d0L, "CSkill::StopRumble xrefs");
        reportReferences(0x0040f2e0L, "TsaPlayAnimationState xrefs");
        reportReferences(0x0040f310L, "TsaPlayAnimationDirect(char*) xrefs");
        reportReferences(0x0040f380L, "TsaPlayAnimationDirect(ResourceName) xrefs");
        reportReferences(0x0040f3f0L, "TsaPushAnimationState xrefs");
        reportReferences(0x0040f420L, "TsaPushAnimationDirect(char*) xrefs");
        reportReferences(0x0040f480L, "TsaPushAnimationDirect(ResourceName) xrefs");
        reportReferences(0x004c89c0L, "FireMissileScripted xrefs");
        reportReferences(0x004c8a00L, "FireMissileScriptedWithAnimation xrefs");
        reportReferences(0x004d3ae0L, "DoDirectDamage xrefs");
        reportReferences(0x004e0460L, "SetAnimationSpeedMultiplier xrefs");
        reportReferences(0x004fcfd0L, "Skill script dispatcher xrefs");
        reportReferences(0x004fd2e0L, "Script invocation helper xrefs");
        reportReferences(0x005c37b0L, "Task scheduler helper xrefs");
        reportReferences(0x005c38d0L, "Script runtime dispatch xrefs");
        reportReferences(0x005c3170L, "Script thread resolver xrefs");
        reportReferences(0x005c41d0L, "Script bytecode step xrefs");
        reportReferences(0x005c4230L, "Bytecode call helper xrefs");
        println("");

        reportSymbols("CSkill");
        reportSymbols("TsaPlayAnimation");
        reportSymbols("TsaPushAnimation");
        reportSymbols("FireMissileScripted");
        reportSymbols("DoDirectDamage");
        reportSymbols("StartAttack");
        reportSymbols("Invulnerable");
        reportSymbols("IsUsingSkill");
        reportFunctions(0x004b4300L, 0x004b5600L);

        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        decompile(decompiler, 0x004b4370L, "CSkill nearby function 1");
        decompile(decompiler, 0x004b4540L, "CSkill nearby function 2");
        decompile(decompiler, 0x004b4650L, "CSkill nearby function 3");
        decompile(decompiler, 0x004b46d0L, "CSkill nearby function 4");
        decompile(decompiler, 0x004b4720L, "CSkill nearby function 5");
        decompile(decompiler, 0x004b47a0L, "CSkill nearby function 6");
        decompile(decompiler, 0x004b4810L, "CSkill::Use");
        decompile(decompiler, 0x004b4bc0L, "CSkill::Use validation helper");
        decompile(decompiler, 0x004b4e80L, "CSkill nearby function 7");
        decompile(decompiler, 0x004b5170L, "Skill script-name helper");
        decompile(decompiler, 0x004b5220L, "CSkill nearby function 8");
        decompile(decompiler, 0x004dc200L, "Actor/model state helper");
        decompile(decompiler, 0x00408980L,
            "CCharacterArbiter::IsInvulnerable");
        decompile(decompiler, 0x004089e0L,
            "CCharacterArbiter::IsUsingSkill");
        decompile(decompiler, 0x004dca10L,
            "CCharacterArbiter::GELSetInvulnerable");
        decompile(decompiler, 0x004b5330L, "Post-queue helper");
        decompile(decompiler, 0x004b5450L, "CSkill nearby function 9");
        decompile(decompiler, 0x004fcfd0L, "Skill script dispatcher");
        decompile(decompiler, 0x004fd2e0L, "Script invocation helper");
        decompile(decompiler, 0x004fd460L, "Script result/task helper");
        decompile(decompiler, 0x005c37b0L, "Task scheduler helper");
        decompile(decompiler, 0x005c07c0L, "Script signature helper");
        decompile(decompiler, 0x005c38d0L, "Script runtime dispatch");
        decompile(decompiler, 0x005c3170L, "Script thread resolver");
        decompile(decompiler, 0x005c41d0L, "Script bytecode step");
        decompile(decompiler, 0x005c4230L, "Bytecode call helper");
        decompile(decompiler, 0x005c42b0L, "Bytecode local lookup helper");
        decompiler.dispose();
    }
}
