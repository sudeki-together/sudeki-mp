// Focused read-only report for title-owned icon slots that may safely host
// SudekiMP's four roster portraits.  It never changes the executable.
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

public class TitlePortraitSlotReport extends GhidraScript {
  private static final String SHA = "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";
  private Address a(long value) { return currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(value); }
  private void dump(DecompInterface d, long value, String role, Set<Address> seen) {
    Function f = currentProgram.getFunctionManager().getFunctionAt(a(value));
    println("\n===== " + role + " " + a(value) + " =====");
    if (f == null || !seen.add(f.getEntryPoint())) { println("missing_or_seen=true"); return; }
    DecompileResults r = d.decompileFunction(f, 120, monitor);
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
      println("callsite=" + r.getFromAddress() + " owner=" + (f == null ? "<none>" : f.getEntryPoint() + " " + f.getName(true)));
      if (++count >= 48) { println("callers_truncated=true"); break; }
    }
    println("callers=" + count);
  }
  @Override public void run() throws Exception {
    if (!SHA.equalsIgnoreCase(currentProgram.getExecutableSHA256())) throw new Exception("unexpected image");
    DecompInterface d = new DecompInterface(); d.openProgram(currentProgram); Set<Address> seen = new HashSet<Address>();
    long[] targets = {
      0x0049f110L, // title frontend constructor: locate controller+0x84 owner
      0x004a0f40L, // state 0xF icon assignment use-site
      0x004a0360L, // title action/state dispatcher
      0x0055b150L, // title cycle-icon/selection helper reached by state 0xF
      0x0055b740L, // title cycle-icon/selection helper reached by state 0xF
      0x0055c070L, // exact resource selector
      0x004a8e80L, // cycle icon constructor
      0x004aa170L  // named UI attachment/config binding
    };
    String[] roles = {
      "title_frontend_constructor", "title_state15_icon_assignment",
      "title_action_dispatcher", "title_icon_helper_a", "title_icon_helper_b",
      "cycle_icon_resource_selector", "cycle_icon_constructor", "ui_anchor_binding"
    };
    for (int i = 0; i < targets.length; ++i) { dump(d, targets[i], roles[i], seen); callers(targets[i], roles[i]); }
    d.dispose();
  }
}
