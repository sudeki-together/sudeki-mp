// Read-only exact-build report for the title controller's +0x84 UI group.
// It maps existing native element offsets and constructors before any title
// roster card reuses a shipped element.
// @category SudekiMP
import ghidra.app.decompiler.*;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import java.util.*;
public class TitleGroupLayoutReport extends GhidraScript {
 private static final String SHA="8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";
 private Address a(long x){return currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(x);}
 private void d(DecompInterface dc,long x,String n,Set<Address>s){Function f=currentProgram.getFunctionManager().getFunctionAt(a(x));println("\n===== "+n+" "+a(x)+" =====");if(f==null||!s.add(f.getEntryPoint()))return;DecompileResults r=dc.decompileFunction(f,300,monitor);println(r.decompileCompleted()?r.getDecompiledFunction().getC():r.getErrorMessage());}
 @Override public void run() throws Exception{if(!SHA.equalsIgnoreCase(currentProgram.getExecutableSHA256()))throw new Exception("unexpected image");DecompInterface dc=new DecompInterface();dc.openProgram(currentProgram);Set<Address>s=new HashSet<Address>();long[]t={0x00559490L,0x004a8e80L,0x0055b150L,0x0055b740L,0x0055c070L,0x0055c190L};String[]n={"title_group_factory","cycle_icon_constructor","title_icon_helper_a","title_icon_helper_b","resource_selector","title_icon_reset"};for(int i=0;i<t.length;i++)d(dc,t[i],n[i],s);dc.dispose();}
}
