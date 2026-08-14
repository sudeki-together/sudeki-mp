// Reports the native Skill Strike activation callers and candidate update path.
// The script is read-only and refuses to inspect an unexpected executable.
// @category SudekiMP

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

public class Phase6ActivationReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private Address address(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(value);
    }

    private void decompileContaining(
            DecompInterface decompiler, long value, String role) {
        Address target = address(value);
        Function function = currentProgram.getFunctionManager().getFunctionContaining(target);
        if (function == null) {
            println(role + ": no containing function at " + target);
            return;
        }
        DecompileResults result = decompiler.decompileFunction(function, 90, monitor);
        println("");
        println("===== " + role + " target=" + target + " function=" +
            function.getEntryPoint() + " " + function.getName(true) + " =====");
        if (!result.decompileCompleted()) {
            println("Decompiler failed: " + result.getErrorMessage());
            return;
        }
        println(result.getDecompiledFunction().getC());
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

    @Override
    protected void run() throws Exception {
        String actualSha256 = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(actualSha256)) {
            throw new Exception("Unexpected executable SHA256: " + actualSha256);
        }

        println("SudekiMP Phase 6 direct-activation report");
        println("SHA256=" + actualSha256);
        println("ImageBase=" + currentProgram.getImageBase());

        reportReferences(0x004b4810L, "CSkill::Use callers");
        reportReferences(0x004b47a0L, "CSkill update/completion references");
        reportReferences(0x00499320L, "Quick Menu skill-selection handler references");
        reportReferences(0x00427bf0L, "Alternate skill activation helper references");
        reportReferences(0x00808d94L, "Active-character owner global references");
        reportReferences(0x006c1c9cL, "ac_QuickSkill0 string references");
        reportReferences(0x006c1cacL, "ac_QuickSkill1 string references");
        reportReferences(0x006c1cbcL, "ac_QuickSkill2 string references");
        reportReferences(0x006c1cccL, "ac_QuickSkill3 string references");
        reportReferences(0x006c1cdcL, "ac_QuickSkill4 string references");
        reportReferences(0x006c1cecL, "ac_QuickSkill5 string references");
        reportReferences(0x004db9b0L, "Skill owner-state predicate references");

        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        decompileContaining(decompiler, 0x004998a1L,
            "Quick Menu CSkill::Use call context");
        decompileContaining(decompiler, 0x00427cb1L,
            "Alternate CSkill::Use call context");
        ReferenceIterator alternateCallers = currentProgram.getReferenceManager()
            .getReferencesTo(address(0x00427bf0L));
        while (alternateCallers.hasNext()) {
            Reference reference = alternateCallers.next();
            if (reference.getReferenceType().isCall()) {
                decompileContaining(decompiler,
                    reference.getFromAddress().getOffset(),
                    "Caller of alternate activation helper");
            }
        }
        decompileContaining(decompiler, 0x004b4810L, "CSkill::Use");
        decompileContaining(decompiler, 0x004b47a0L,
            "CSkill update/completion candidate");
        decompileContaining(decompiler, 0x004b4bc0L,
            "Pre-use skill-state query");
        decompileContaining(decompiler, 0x004db9b0L,
            "Skill owner-state predicate");
        decompileContaining(decompiler, 0x004b4c90L,
            "Skill validation failure message mapper");
        decompileContaining(decompiler, 0x00499320L,
            "Quick Menu skill-selection handler");
        decompiler.dispose();
    }
}
