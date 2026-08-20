// Focused read-only portrait construction and title-UI registration report.
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

public class PortraitRosterUiReport extends GhidraScript {
  private static final String SHA = "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";
  private Address a(long value) { return currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(value); }
  private void dump(DecompInterface d, long value, String role, Set<Address> seen) {
    Function f = currentProgram.getFunctionManager().getFunctionAt(a(value));
    println("\n===== " + role + " " + a(value) + " =====");
    if (f == null || !seen.add(f.getEntryPoint())) { println("missing_or_seen=true"); return; }
    DecompileResults r = d.decompileFunction(f, 90, monitor);
    println(r.decompileCompleted() ? r.getDecompiledFunction().getC() : r.getErrorMessage());
  }
  private void callers(long value, String role) {
    println("\nCALLERS " + role + " " + a(value));
    ReferenceIterator it = currentProgram.getReferenceManager().getReferencesTo(a(value));
    int count = 0;
    while (it.hasNext() && !monitor.isCancelled()) {
      Reference r = it.next();
      if (!r.getReferenceType().isCall()) continue;
      Function f = currentProgram.getFunctionManager().getFunctionContaining(r.getFromAddress());
      println("callsite=" + r.getFromAddress() + " owner=" + (f == null ? "<none>" : f.getEntryPoint()));
      if (++count == 32) break;
    }
  }
  @Override public void run() throws Exception {
    if (!SHA.equalsIgnoreCase(currentProgram.getExecutableSHA256())) throw new Exception("unexpected image");
    DecompInterface d = new DecompInterface(); d.openProgram(currentProgram); Set<Address> seen = new HashSet<Address>();
    long[] targets = {0x004a0f40L,0x00559490L,0x005592f0L,0x0055be70L,0x0055c0e0L,0x004a94f0L,0x004a8df0L,0x004a8e80L,0x00581ec0L,0x004a9060L,0x004aa170L,0x0055c070L};
    String[] roles = {"pc_front_end_state_update","portrait_group_factory","portrait_group_bind","cycle_icon_bind","cycle_icon_resource_assignment","portrait_gizmo_teardown","portrait_gizmo_setup","cycle_icon_constructor","portrait_gizmo_constructor","portrait_gizmo_update","ui_element_name_binding","cycle_icon_resource_selector"};
    for (int i=0;i<targets.length;i++) { dump(d,targets[i],roles[i],seen); callers(targets[i],roles[i]); }
    d.dispose();
  }
}
