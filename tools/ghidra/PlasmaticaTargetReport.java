// Reports the target-retention path used by Plasmatica and scripted missiles.
// The script is read-only and refuses to inspect an unexpected executable.
// @category SudekiMP

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

public class PlasmaticaTargetReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private Address address(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(value);
    }

    private void reportReferences(long value, String role) {
        ReferenceIterator references = currentProgram.getReferenceManager().getReferencesTo(
            address(value)
        );
        int count = 0;
        println(role + " target=" + address(value));
        while (references.hasNext()) {
            Reference reference = references.next();
            Function caller = currentProgram.getFunctionManager().getFunctionContaining(
                reference.getFromAddress()
            );
            println(String.format("  from=%s type=%s function=%s",
                reference.getFromAddress(), reference.getReferenceType(),
                caller == null ? "<none>" : caller.getName(true)));
            count++;
        }
        println("  references=" + count);
    }

    private void decompile(DecompInterface decompiler, long value, String role) {
        Function function = currentProgram.getFunctionManager().getFunctionAt(address(value));
        if (function == null) {
            println(role + ": no function at " + address(value));
            return;
        }
        DecompileResults result = decompiler.decompileFunction(function, 90, monitor);
        println("");
        println("===== " + role + " " + function.getEntryPoint() + " " +
            function.getName(true) + " =====");
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

        println("SudekiMP Plasmatica target-retention report");
        println("SHA256=" + actualSha256);
        println("ImageBase=" + currentProgram.getImageBase());

        reportReferences(0x004b9e00L, "CTargeter skill-target getter");
        reportReferences(0x004b9e20L, "CTargeter begin skill targeting");
        reportReferences(0x004b9ef0L, "CTargeter end skill targeting");
        reportReferences(0x004b9dc0L, "CTargeter current-target getter");
        reportReferences(0x00429570L, "Gamepad skill-targeting mode switch");
        reportReferences(0x004c7aa0L, "Missile launch-direction resolver");
        reportReferences(0x004c89c0L, "CMissileManager scripted fire");
        reportReferences(0x007c3b44L, "Current skill-target GEL pointer global");
        reportReferences(0x007c2fcdL, "Skill-targeting active global");

        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        decompile(decompiler, 0x004b9e00L,
            "CTargeter::GetSkillTargettingModeTarget");
        decompile(decompiler, 0x004b9e20L,
            "CTargeter::StartSkillTargetting");
        decompile(decompiler, 0x004b9ef0L,
            "CTargeter::EndSkillTargetting");
        decompile(decompiler, 0x004b9dc0L,
            "CTargeter::GetGelCurrentTarget");
        decompile(decompiler, 0x004b9cc0L,
            "CTargeter::EnableAutoTargetting");
        decompile(decompiler, 0x00429570L,
            "CGamePadControlComponent::EnableSkillTargettingMode");
        decompile(decompiler, 0x00429610L,
            "CGamePadControlComponent::ShouldSkillTargettingModeBeActive");
        decompile(decompiler, 0x004b84f0L,
            "CTargeter selected skill-target writer");
        decompile(decompiler, 0x004b8200L,
            "CTargeter skill-targeting active reader");
        decompile(decompiler, 0x004b9ab0L,
            "CTargeter skill-targeting update");
        decompile(decompiler, 0x004c79c0L,
            "CMissileManager resolve launch origin from owner targeter");
        decompile(decompiler, 0x004c7aa0L,
            "CMissileManager resolve launch direction");
        decompile(decompiler, 0x004c7d20L,
            "CMissileManager resolve targeter attachment");
        decompile(decompiler, 0x004c7160L,
            "CMissileManager launch selected missile");
        decompile(decompiler, 0x004c89c0L,
            "CMissileManager::FireMissileScripted");
        decompile(decompiler, 0x004cc060L,
            "CEntityAttacks::StartAttack");
        decompile(decompiler, 0x004cc0a0L,
            "CEntityAttacks::StartAttackOnTarget");
        decompiler.dispose();
    }
}
