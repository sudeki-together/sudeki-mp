// Read-only trace for the UI resource loader and simple icons that call it.
// Goal: determine whether a native UI texture can be borrowed as a D3D surface
// for SudekiMP's title overlay without extracting it from SOLData.baf.
// @category SudekiMP
import ghidra.app.decompiler.*;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import java.util.HashSet;
import java.util.Set;
public class UiTextureSurfaceReport extends GhidraScript {
 private static final String SHA="8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";
 private Address a(long x){return currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(x);}
 private void d(DecompInterface dc,long x,String n,Set<Address>s){Function f=currentProgram.getFunctionManager().getFunctionAt(a(x));println("\n===== "+n+" "+a(x)+" =====");if(f==null||!s.add(f.getEntryPoint())){println("missing_or_seen=true");return;}DecompileResults r=dc.decompileFunction(f,180,monitor);println(r.decompileCompleted()?r.getDecompiledFunction().getC():r.getErrorMessage());}
 @Override public void run() throws Exception{if(!SHA.equalsIgnoreCase(currentProgram.getExecutableSHA256()))throw new Exception("unexpected image");DecompInterface dc=new DecompInterface();dc.openProgram(currentProgram);Set<Address>s=new HashSet<Address>();long[] t={0x005d92e0L,0x005d8d70L,0x005d91b0L,0x00529a50L,0x004a6f40L,0x004abb80L,0x0055c0e0L,0x00558fb0L,0x00559110L,0x00559280L};String[]n={"async_ui_resource_loader","ui_loader_job_setup","ui_loader_callback_contract","simple_cycle_icon_owner","title_simple_icon_owner","icon_resource_owner","cycle_assignment","ui_element_registry","name_to_element_id","ui_element_collection"};for(int i=0;i<t.length;i++)d(dc,t[i],n[i],s);dc.dispose();}
}
