// Read-only exact-image contracts for the Dev Play avatar research spike.
// @category SudekiMP
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

public class DevPlayAvatarContractReport extends GhidraScript {
    private static final String SHA = "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";
    @Override public void run() throws Exception {
        if (!SHA.equalsIgnoreCase(currentProgram.getExecutableSHA256()))
            throw new Exception("unsupported executable");
        long[] entries = {0x4d1b00L,0x4d21d0L,0x4d7c10L,0x4d8790L,
            0x4d92d0L,0x4b1b00L,0x4b20d0L,0x4b1900L,0x4b1310L,
            0x437170L,0x42a370L,0x535040L,0x4e84c0L,0x55c070L,
            0x4b1150L,0x4b1530L,0x4d37d0L,0x4d1cb0L,0x4d95c0L,0x4d9630L};
        String[] roles = {"strike_prepare", "accepted_damage", "equip_item",
            "exported_set_weapon", "weapon_presentation_state", "spawn_pc",
            "spawn_entity", "spawn_common", "spawn_lookup", "camera_set_target",
            "camera_character_reassignment", "camera_entity_target_create",
            "camera_install_target", "portrait_selector", "spawn_job_constructor",
            "spawn_job_completion", "strike_base_calculation", "strike_damage_calculation",
            "weapon_packet_effects", "weapon_packet_modifiers"};
        DecompInterface d = new DecompInterface(); d.openProgram(currentProgram);
        try {
            for (int i=0;i<entries.length;i++) {
                Address a=currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(entries[i]);
                Function f=getFunctionAt(a);
                println("\n===== "+roles[i]+" "+a+" =====");
                if(f==null) { println("missing=true"); continue; }
                println("signature="+f.getSignature()+" convention="+f.getCallingConventionName());
                DecompileResults r=d.decompileFunction(f,120,monitor);
                println(r.decompileCompleted()?r.getDecompiledFunction().getC():r.getErrorMessage());
                ReferenceIterator refs=currentProgram.getReferenceManager().getReferencesTo(a);
                while(refs.hasNext()) {
                    Reference ref=refs.next();
                    if(ref.getReferenceType().isCall()) println("callsite="+ref.getFromAddress());
                }
            }
        } finally { d.dispose(); }
    }
}
