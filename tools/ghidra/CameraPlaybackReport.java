// Reports native camera playback controls relevant to Plasmatica.
// The script is read-only and refuses to inspect an unexpected executable.
// @category SudekiMP

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;

public class CameraPlaybackReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private Address address(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
    }

    private void reportReferences(long value, String role) {
        ReferenceIterator references = currentProgram.getReferenceManager()
            .getReferencesTo(address(value));
        int count = 0;
        println("\n===== " + role + " references =====");
        while (references.hasNext()) {
            Reference reference = references.next();
            Function caller = currentProgram.getFunctionManager()
                .getFunctionContaining(reference.getFromAddress());
            println("  " + reference.getFromAddress() + "  "
                + reference.getReferenceType() + "  caller="
                + (caller == null ? "<data/non-function>"
                    : caller.getName(true)));
            count++;
        }
        println("  count=" + count);
    }

    private void reportSymbols(String needle) {
        println("\n===== Symbols containing '" + needle + "' =====");
        SymbolIterator symbols = currentProgram.getSymbolTable()
            .getAllSymbols(true);
        int count = 0;
        while (symbols.hasNext()) {
            Symbol symbol = symbols.next();
            if (symbol.getName(true).toLowerCase().contains(
                    needle.toLowerCase())) {
                println("  " + symbol.getAddress() + "  "
                    + symbol.getName(true));
                count++;
            }
        }
        println("  count=" + count);
    }

    private void decompile(DecompInterface decompiler, long value,
            String role) {
        Function function = currentProgram.getFunctionManager()
            .getFunctionContaining(address(value));
        if (function == null) {
            println(role + ": no function at " + address(value));
            return;
        }
        DecompileResults result = decompiler.decompileFunction(
            function, 60, monitor);
        println("\n===== " + role + " " + function.getEntryPoint()
            + " " + function.getName(true) + " =====");
        if (!result.decompileCompleted()) {
            println("Decompiler failed: " + result.getErrorMessage());
            return;
        }
        println(result.getDecompiledFunction().getC());
    }

    @Override
    protected void run() throws Exception {
        String actualSha256 = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(actualSha256)) {
            throw new Exception("Unexpected executable SHA256: "
                + actualSha256);
        }

        println("SudekiMP camera playback static report");
        println("SHA256=" + actualSha256);
        println("ImageBase=" + currentProgram.getImageBase());

        long[] functions = {
            0x00412910L,
            0x00411e30L,
            0x00412060L,
            0x00438810L,
            0x00438280L,
            0x00437fc0L,
            0x00438170L,
            0x00437ed0L,
            0x00412650L
        };
        String[] roles = {
            "CSpiritCam::StartCam",
            "CSpiritCam internal camera start",
            "CSpiritCam camera playback setup",
            "PlayCameraSequence",
            "PlaySpline overload 1",
            "PlaySpline overload 2",
            "PlaySpline overload 3",
            "PlaySpline overload 4",
            "CSpiritCam::TestCameraCollision"
        };

        for (int index = 0; index < functions.length; ++index) {
            reportReferences(functions[index], roles[index]);
        }
        reportSymbols("PlaybackSequence");
        reportSymbols("CSpiritCam");
        reportSymbols("PlaySpline");
        reportSymbols("CameraSequence");
        reportReferences(0x005538a0L,
            "PlaybackSequence current-element updater");

        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        for (int index = 0; index < functions.length; ++index) {
            decompile(decompiler, functions[index], roles[index]);
        }
        long[] playbackStateMethods = {
            0x0058ca20L,
            0x005a64f0L,
            0x005a6550L,
            0x005a6590L,
            0x004cedd0L,
            0x005a6620L,
            0x005a1350L,
            0x005a6090L,
            0x005a6670L,
            0x00534610L,
            0x005346b0L,
            0x00534660L,
            0x005346f0L,
            0x00534780L,
            0x00534730L,
            0x005347c0L,
            0x005a4ec0L
        };
        for (int index = 0; index < playbackStateMethods.length; ++index) {
            decompile(decompiler, playbackStateMethods[index],
                "PlaybackSequenceState vtable[" + index + "]");
        }
        decompile(decompiler, 0x005538a0L,
            "PlaybackSequence current-element updater");
        decompiler.dispose();
    }
}
