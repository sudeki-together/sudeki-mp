// Read-only exact-build report for the last cD3DTexture decode/upload path.
// @category SudekiMP
import ghidra.app.decompiler.*;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import java.util.*;
public class D3DTextureDecodeReport extends GhidraScript {
 private static final String SHA="8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";
 private Address a(long x){return currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(x);}
 private void d(DecompInterface dc,long x,String n,Set<Address>s){Function f=currentProgram.getFunctionManager().getFunctionAt(a(x));println("\n===== "+n+" "+a(x)+" =====");if(f==null||!s.add(f.getEntryPoint()))return;DecompileResults r=dc.decompileFunction(f,360,monitor);println(r.decompileCompleted()?r.getDecompiledFunction().getC():r.getErrorMessage());}
 @Override public void run() throws Exception{if(!SHA.equalsIgnoreCase(currentProgram.getExecutableSHA256()))throw new Exception("unexpected image");DecompInterface dc=new DecompInterface();dc.openProgram(currentProgram);Set<Address>s=new HashSet<Address>();long[]t={0x005d8300L,0x005d8690L,0x005d8a40L,0x005d8be0L,0x005d8290L,0x005eddc0L};String[]n={"texture_decode_dispatch","texture_decode_complete","texture_queue_drain","texture_queue_add","texture_backend_reset","d3d_object_release"};for(int i=0;i<t.length;i++)d(dc,t[i],n[i],s);dc.dispose();}
}
