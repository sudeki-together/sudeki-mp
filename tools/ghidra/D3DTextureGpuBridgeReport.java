// Read-only exact-build report for the texture decoder-to-GPU bridge.
// @category SudekiMP
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import java.util.HashSet;
import java.util.Set;

public class D3DTextureGpuBridgeReport extends GhidraScript {
  private static final String SHA = "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";
  private Address a(long v) { return currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(v); }
  private void dump(DecompInterface d, long v, String n, Set<Address> seen) {
    Function f=currentProgram.getFunctionManager().getFunctionAt(a(v));
    println("\n===== "+n+" "+a(v)+" =====");
    if(f==null||!seen.add(f.getEntryPoint())) { println("missing_or_seen=true"); return; }
    DecompileResults r=d.decompileFunction(f,120,monitor);
    println(r.decompileCompleted()?r.getDecompiledFunction().getC():r.getErrorMessage());
  }
  @Override public void run() throws Exception {
    if(!SHA.equalsIgnoreCase(currentProgram.getExecutableSHA256())) throw new Exception("unexpected image");
    DecompInterface d=new DecompInterface(); d.openProgram(currentProgram); Set<Address> seen=new HashSet<Address>();
    long[] a={0x005d8300L,0x005d8690L,0x005f3780L,0x005f3890L,0x005f3ae0L,0x005f3a50L,0x005f3980L,0x005d66f0L,0x005fb720L};
    String[] n={"codec_dispatch","texture_upload_finish","decode_mip_layout","decode_cube_layout","texture_create_or_upload","format_compatible","format_size","texture_name","render_lock_token"};
    for(int i=0;i<a.length;i++) dump(d,a[i],n[i],seen);
    d.dispose();
  }
}
