// Reports native player/entity spawn and removal APIs for the SudekiMP cleanroom.
// Read-only: refuses to inspect any executable other than the supported GOG build.
// @category SudekiMP

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;

public class CleanroomSpawnReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private Address address(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
    }

    private void decompile(DecompInterface decompiler, long target, String role) {
        Function function = currentProgram.getFunctionManager()
            .getFunctionAt(address(target));
        println("");
        println("===== " + role + " " + address(target) + " " +
            (function == null ? "<none>" : function.getName(true)) + " =====");
        if (function == null) {
            return;
        }
        DecompileResults result = decompiler.decompileFunction(
            function, 120, monitor);
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
        println("SudekiMP cleanroom spawn/removal report");
        println("SHA256=" + actualSha256);
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        long[] targets = {
            0x004b1b00L, 0x004b1ca0L,
            0x004b23a0L, 0x004b2520L, 0x004b2300L,
            0x00423230L, 0x00423390L, 0x004235e0L, 0x00424e40L,
            0x004f6170L, 0x004f61d0L,
            0x00504480L, 0x005044a0L,
            0x004307b0L,
            0x004b1e70L, 0x004b20d0L,
            0x004049c0L, 0x0043eb80L, 0x004b1310L,
            0x005b9440L, 0x005b9760L
        };
        String[] roles = {
            "InternalSpawnPC(ResourceName, xyz)",
            "InternalSpawnPC(ResourceName, ResourceName)",
            "RemovePC(ResourceName)",
            "DeletePC(ResourceName)",
            "DespawnEntity(TPtr<Entity>)",
            "CGroupPlayers::AddPlayer",
            "CGroupPlayers::RemovePlayer",
            "CGroupPlayers::RemovePlayerAndDelete",
            "CGroupPlayers::SetAsLeadPlayer",
            "AiPCFormationAddGroup",
            "AiPCFormationDelGroup",
            "GetPC(char const *)",
            "GetPC(ResourceName)",
            "SetPCsCanDie(bool)",
            "SpawnEntity(name, ResourceName, ResourceName)",
            "SpawnEntity(name, xyz)",
            "String text assignment helper (not ResourceName)",
            "String/type to ResourceName adapter",
            "ResourceName canonicalization before player spawn",
            "ResourceName text constructor",
            "ResourceName reference release"
        };
        for (int index = 0; index < targets.length; ++index) {
            decompile(decompiler, targets[index], roles[index]);
        }
        decompiler.dispose();
    }
}
