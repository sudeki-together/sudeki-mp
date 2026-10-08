// Read-only exact-image weapon/item ownership and equip contract report.
// Keep generated decompilation private; publish sanitized conclusions only.
// @category SudekiMP
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;

public class DevPlayWeaponEquipReport extends GhidraScript {
    private static final String SHA = "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";
    @Override public void run() throws Exception {
        if (!SHA.equalsIgnoreCase(currentProgram.getExecutableSHA256()))
            throw new Exception("unsupported executable");
        DecompInterface d = new DecompInterface();
        d.openProgram(currentProgram);
        try {
            Address table=toAddr(0x6d3a28L);
            for (int slot=0;slot<0x38;slot+=4) {
                Address target=toAddr(Integer.toUnsignedLong(getInt(table.add(slot))));
                println("weapon_item_vtable_slot="+slot+" target="+target+
                    " function="+getFunctionAt(target));
            }
            long[] entries={
                0x4015b0L,0x4015e0L, // Intrusive by-value actor handle.
                0x41fa70L,0x41fd10L,0x41ff50L,0x420000L,0x4201e0L,
                0x421ce0L,0x479b00L, // Item DB construction, borrow, shutdown.
                0x52fa90L,0x52fbf0L,0x52fe00L,0x52fef0L,
                0x52e0a0L,0x52e140L,0x52e1e0L, // Native availability dispatch.
                0x4d7540L,0x4d7630L,0x4d8010L, // Authored fallback/auto-equip.
                0x4d73c0L,0x4d7880L,0x4d79a0L,0x4d7c10L,
                0x4d7e30L,0x4d80b0L,0x4d8470L,0x4d87d0L,
                0x4d8be0L,0x4d8fb0L,0x4d9240L,0x4d92d0L,
                0x523500L,0x511b30L, // Model and wrapper ownership.
                0x4d95c0L,0x4d1cb0L // Native packet/HP damage inputs.
            };
            for (long entry:entries) {
                Address a=toAddr(entry);
                Function f=getFunctionAt(a);
                println("entry="+a+" function="+f);
                if(f==null) continue;
                println("signature="+f.getSignature()+" body="+f.getBody());
                var result=d.decompileFunction(f,90,monitor);
                println(result.decompileCompleted()?result.getDecompiledFunction().getC():result.getErrorMessage());
                var refs=currentProgram.getReferenceManager().getReferencesTo(a);
                while(refs.hasNext()) {
                    var ref=refs.next();
                    if(ref.getReferenceType().isCall())
                        println("caller="+ref.getFromAddress()+" owner="+getFunctionContaining(ref.getFromAddress()));
                }
            }
        } finally { d.dispose(); }
    }
}
