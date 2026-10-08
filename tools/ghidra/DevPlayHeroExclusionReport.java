// Read-only exact-image research for native Dev Play party replacement.
// Does not authorize native calls, actor deletion, or save changes.
// @category SudekiMP
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.program.model.listing.Function;
public class DevPlayHeroExclusionReport extends GhidraScript {
    public void run() throws Exception {
        if (!"8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94".equalsIgnoreCase(currentProgram.getExecutableSHA256()))
            throw new Exception("unsupported executable");
        DecompInterface d = new DecompInterface(); d.openProgram(currentProgram);
        try {
            for (long entry : new long[]{
                    0x4b23a0L,0x4b2520L,0x4b2300L,
                    0x423230L,0x423280L,0x423390L,0x4233e0L,0x4235e0L,0x424e40L,
                    0x423750L,0x4237b0L,0x427cf0L,0x429370L,0x4ef700L,
                    0x4b2cb0L,0x4f6170L,0x4f61d0L,0x4b1530L,0x42a370L,
                    0x401c20L,0x4019c0L,0x401b30L,0x401b50L,
                    0x423080L,0x424060L,0x423b50L,0x43e840L,0x43e970L,
                    0x4f2b00L,0x4f2b30L,0x4b3dd0L,0x4ec2d0L,
                    0x4ea7c0L,0x4ea940L,0x4b9680L,0x529780L,
                    0x40b360L,0x40b3d0L,0x49c930L,0x4a63a0L,
                    0x581c90L,0x581b20L,0x4c8d20L,0x4bcc30L,0x4bcc60L,
                    0x501ea3L,0x43f170L,0x5132b0L,0x596f20L,
                    0x5b9440L,0x5b96f0L,0x43a4a0L,0x5f54d0L}) {
                Function f = getFunctionContaining(toAddr(entry));
                println("FUNCTION " + toAddr(entry) + " " + f);
                if (f == null) throw new Exception("missing function " + toAddr(entry));
                println("BODY " + f.getBody() + " SIGNATURE " + f.getSignature());
                var result = d.decompileFunction(f,120,monitor);
                if (!result.decompileCompleted()) throw new Exception(result.getErrorMessage());
                println(result.getDecompiledFunction().getC());
                var refs = currentProgram.getReferenceManager().getReferencesTo(f.getEntryPoint());
                int calls = 0;
                while (refs.hasNext()) {
                    var ref = refs.next();
                    if (ref.getReferenceType().isCall()) {
                        if (calls < 40) println("CALLER " + ref.getFromAddress() + " " + getFunctionContaining(ref.getFromAddress()));
                        ++calls;
                    }
                }
                println("CALLER_COUNT " + calls);
            }
            println("DevPlayHeroExclusionReport complete");
        } finally { d.dispose(); }
    }
}
