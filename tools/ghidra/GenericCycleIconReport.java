// Exact-build, read-only report for a standalone UIElementCycleIcon lifecycle.
// It is intentionally smaller than the portrait report so generic construction,
// resource selection, and render/attachment ownership remain visible.
// @category SudekiMP
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;
import java.util.HashSet;
import java.util.Set;
public class GenericCycleIconReport extends GhidraScript {
  private static final String SHA = "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";
  private Address a(long x) { return currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(x); }
  private void dump(DecompInterface d,long x,String role,Set<Address> seen) {
    Function f=currentProgram.getFunctionManager().getFunctionAt(a(x));
    println("\n===== "+role+" "+a(x)+" =====");
    if(f==null||!seen.add(f.getEntryPoint())) { println("missing_or_seen=true"); return; }
    DecompileResults r=d.decompileFunction(f,180,monitor);
    println(r.decompileCompleted()?r.getDecompiledFunction().getC():r.getErrorMessage());
  }
  private void callers(long x,String role) {
    println("\nCALLERS "+role+" "+a(x)); ReferenceIterator it=currentProgram.getReferenceManager().getReferencesTo(a(x)); int n=0;
    while(it.hasNext()&&!monitor.isCancelled()) { Reference r=it.next(); if(!r.getReferenceType().isCall()) continue; Function f=currentProgram.getFunctionManager().getFunctionContaining(r.getFromAddress()); println("callsite="+r.getFromAddress()+" owner="+(f==null?"<none>":f.getEntryPoint())); if(++n>=32)break; }
    println("callers="+n);
  }
  @Override public void run() throws Exception {
    if(!SHA.equalsIgnoreCase(currentProgram.getExecutableSHA256()))throw new Exception("unexpected image");
    DecompInterface d=new DecompInterface();d.openProgram(currentProgram);Set<Address> seen=new HashSet<Address>();
    long[] t={0x00581ec0L,0x0055c0e0L,0x0055c070L,0x0055c020L,0x0055be70L,0x0055e2d0L,0x0055c450L,0x004a8fc0L};
    String[] r={"cycle_icon_constructor","cycle_icon_resource_assignment","resource_table_selector","cycle_icon_vfunc4","named_anchor_bind","cycle_icon_group_update","cycle_icon_state_refresh","indexed_ui_binding"};
    for(int i=0;i<t.length;i++){dump(d,t[i],r[i],seen);callers(t[i],r[i]);}
    d.dispose();
  }
}
