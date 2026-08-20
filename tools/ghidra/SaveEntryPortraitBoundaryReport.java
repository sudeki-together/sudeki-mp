// Read-only report for the native save/load portrait update boundary.
// @category SudekiMP
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

public class SaveEntryPortraitBoundaryReport extends GhidraScript {
  private static final String SHA = "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";
  private Address a(long value) { return currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(value); }
  private void dump(DecompInterface d, long value, String role) {
    Function f = currentProgram.getFunctionManager().getFunctionAt(a(value));
    println("\n===== " + role + " " + a(value) + " =====");
    if (f == null) { println("missing=true"); return; }
    DecompileResults r = d.decompileFunction(f, 180, monitor);
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
      if (++count >= 64) { println("callers_truncated=true"); break; }
    }
    println("callers=" + count);
  }
  @Override public void run() throws Exception {
    if (!SHA.equalsIgnoreCase(currentProgram.getExecutableSHA256())) throw new Exception("unexpected image");
    DecompInterface d = new DecompInterface(); d.openProgram(currentProgram);
    long[] targets = {
      0x0048c710L, 0x0048c160L, 0x004898a0L, 0x0048c4f0L, 0x00489954L, 0x0048e0e0L,
      0x0055e2d0L, 0x0048e8a0L, 0x0048ee70L, 0x0055c070L, 0x0055c0e0L,
      0x004a0360L, 0x004a0f40L, 0x004a1950L
    };
    String[] roles = {
      "save_entry_update", "save_entry_owner", "save_page_owner", "save_page_builder", "save_page_dispatch",
      "save_ui_update_a", "portrait_group_runtime_update", "save_ui_update_b", "save_ui_update_c",
      "portrait_selector", "portrait_assignment", "front_end_action",
      "front_end_state_update", "front_end_menu_builder"
    };
    for (int i = 0; i < targets.length; ++i) { dump(d, targets[i], roles[i]); callers(targets[i], roles[i]); }
    d.dispose();
  }
}
