// Read-only exact-build report for the native texture creation path.
// It follows the backend calls after a cD3DTexture becomes resident and
// records any direct IDirect3DTexture9 creation/binding boundary.
// @category SudekiMP
import ghidra.app.decompiler.*;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import java.util.*;
public class D3DTextureCreateReport extends GhidraScript {
 private static final String SHA="8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";
 private Address a(long x){return currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(x);}
 private void d(DecompInterface dc,long x,String n,Set<Address>s){Function f=currentProgram.getFunctionManager().getFunctionAt(a(x));println("\n===== "+n+" "+a(x)+" =====");if(f==null||!s.add(f.getEntryPoint()))return;DecompileResults r=dc.decompileFunction(f,300,monitor);println(r.decompileCompleted()?r.getDecompiledFunction().getC():r.getErrorMessage());}
 @Override public void run() throws Exception{if(!SHA.equalsIgnoreCase(currentProgram.getExecutableSHA256()))throw new Exception("unexpected image");DecompInterface dc=new DecompInterface();dc.openProgram(currentProgram);Set<Address>s=new HashSet<Address>();long[]t={0x005d71d0L,0x005d7230L,0x005d7320L,0x005d8b50L,0x005f32f0L,0x005f45c0L,0x005f4760L,0x005eac20L,0x005eae20L};String[]n={"texture_upload_or_create","texture_post_upload","texture_finalize","texture_resource_prepare","texture_destroy","resource_job_wait","resource_job_release","d3d_work_signal","d3d_work_wait"};for(int i=0;i<t.length;i++)d(dc,t[i],n[i],s);dc.dispose();}
}
