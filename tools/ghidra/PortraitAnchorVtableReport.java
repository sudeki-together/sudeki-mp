// Exact-build, read-only report for the live ICON_PORTRAIT scene anchors.
// @category SudekiMP
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;

public class PortraitAnchorVtableReport extends GhidraScript {
    private static final String SHA =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";
    private Address a(long v) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(v);
    }
    private long u32(long v) throws Exception {
        return Integer.toUnsignedLong(getInt(a(v)));
    }
    private void dump(DecompInterface dc, long value, String role) {
        Function f = currentProgram.getFunctionManager().getFunctionAt(a(value));
        println("\n===== " + role + " " + a(value) + " =====");
        if (f == null) { println("missing=true"); return; }
        DecompileResults r = dc.decompileFunction(f, 240, monitor);
        println("function=" + f.getName(true));
        println(r.decompileCompleted() ? r.getDecompiledFunction().getC() :
            r.getErrorMessage());
    }
    @Override
    public void run() throws Exception {
        if (!SHA.equalsIgnoreCase(currentProgram.getExecutableSHA256()))
            throw new Exception("unexpected image");
        final long vtable = 0x006deb7cL;
        DecompInterface dc = new DecompInterface();
        dc.openProgram(currentProgram);
        println("portrait_anchor_vtable=" + a(vtable));
        println("complete_object_locator=" + a(u32(vtable - 4)));
        for (int slot = 0; slot < 32; ++slot) {
            long target = u32(vtable + slot * 4L);
            println("slot=0x" + Integer.toHexString(slot * 4) +
                " target=" + a(target));
            dump(dc, target, "anchor vtable+0x" +
                Integer.toHexString(slot * 4));
        }
        dc.dispose();
    }
}
