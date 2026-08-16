// Traces Sudeki's ranged first-person model/weapon presentation boundary.
// Read-only: refuses to inspect any executable other than the supported GOG build.
// @category SudekiMP

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.scalar.Scalar;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;

import java.util.HashSet;
import java.util.Set;

public class FirstPersonPresentationReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private Address address(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
    }

    private String decompileText(
            DecompInterface decompiler,
            Function function) {
        if (function == null) {
            return null;
        }
        DecompileResults result = decompiler.decompileFunction(
            function, 90, monitor);
        if (!result.decompileCompleted()) {
            return null;
        }
        return result.getDecompiledFunction().getC();
    }

    private void decompile(
            DecompInterface decompiler,
            Function function,
            String role,
            Set<Address> seen) {
        if (function == null || !seen.add(function.getEntryPoint())) {
            return;
        }
        String text = decompileText(decompiler, function);
        println("");
        println("===== " + role + " function=" + function.getEntryPoint() +
            " " + function.getName(true) + " =====");
        println(text == null ? "Decompiler failed" : text);
    }

    private void reportCallers(
            DecompInterface decompiler,
            Function function,
            String role,
            Set<Address> seen) {
        if (function == null) {
            return;
        }
        println("");
        println("CALLERS role=" + role + " function=" +
            function.getEntryPoint());
        ReferenceIterator references = currentProgram.getReferenceManager()
            .getReferencesTo(function.getEntryPoint());
        while (references.hasNext() && !monitor.isCancelled()) {
            Reference reference = references.next();
            if (!reference.getReferenceType().isCall()) {
                continue;
            }
            Function caller = currentProgram.getFunctionManager()
                .getFunctionContaining(reference.getFromAddress());
            println("  callsite=" + reference.getFromAddress() + " caller=" +
                (caller == null ? "<none>" : caller.getEntryPoint() + " " +
                    caller.getName(true)));
            decompile(decompiler, caller, "caller of " + role, seen);
        }
    }

    private void reportFirstPersonSymbols() {
        println("");
        println("MATCHING SYMBOLS");
        SymbolIterator symbols = currentProgram.getSymbolTable().getAllSymbols(true);
        while (symbols.hasNext() && !monitor.isCancelled()) {
            Symbol symbol = symbols.next();
            String lower = symbol.getName(true).toLowerCase();
            if (lower.contains("firstperson") ||
                lower.contains("weaponvisible") ||
                lower.contains("simplegamemodelinterface::enable") ||
                lower.contains("simplegamemodelinterface::setroflags") ||
                lower.contains("simplegamemodelinterface::clrroflags")) {
                println("  " + symbol.getAddress() + " " +
                    symbol.getName(true) + " type=" + symbol.getSymbolType());
            }
        }
    }

    private void reportDataUsers(
            DecompInterface decompiler,
            long target,
            String role,
            Set<Address> seen) {
        Address destination = address(target);
        println("");
        println("REFERENCES role=" + role + " target=" + destination);
        ReferenceIterator references = currentProgram.getReferenceManager()
            .getReferencesTo(destination);
        while (references.hasNext() && !monitor.isCancelled()) {
            Reference reference = references.next();
            Function caller = currentProgram.getFunctionManager()
                .getFunctionContaining(reference.getFromAddress());
            println("  from=" + reference.getFromAddress() + " type=" +
                reference.getReferenceType() + " caller=" +
                (caller == null ? "<none>" : caller.getEntryPoint() + " " +
                    caller.getName(true)));
            decompile(decompiler, caller, "user of " + role, seen);
        }
    }

    private void reportArbiterFirstPersonFieldUsers(
            DecompInterface decompiler,
            Set<Address> seen) {
        Set<Address> candidates = new HashSet<Address>();
        InstructionIterator instructions = currentProgram.getListing()
            .getInstructions(true);
        while (instructions.hasNext() && !monitor.isCancelled()) {
            Instruction instruction = instructions.next();
            for (int operand = 0; operand < instruction.getNumOperands(); ++operand) {
                Scalar scalar = instruction.getScalar(operand);
                if (scalar != null && scalar.getUnsignedValue() == 0x60L) {
                    Function function = currentProgram.getFunctionManager()
                        .getFunctionContaining(instruction.getAddress());
                    if (function != null) {
                        candidates.add(function.getEntryPoint());
                    }
                }
            }
        }
        println("");
        println("INLINE ARBITER+0x60 BIT-0 CANDIDATES");
        for (Address entry : candidates) {
            Function function = currentProgram.getFunctionManager()
                .getFunctionAt(entry);
            String text = decompileText(decompiler, function);
            if (text == null ||
                !(text.contains("+ 0x60") || text.contains("+ 0x60)")) ||
                !(text.contains("& 1") || text.contains("| 1") ||
                  text.contains("^ 1"))) {
                continue;
            }
            println("  function=" + function.getEntryPoint() + " " +
                function.getName(true));
            decompile(decompiler, function,
                "inline arbiter+0x60 bit-0 user", seen);
        }
    }

    private void reportVisibilityDistanceCandidates(
            DecompInterface decompiler,
            Set<Address> seen) {
        Set<Address> candidates = new HashSet<Address>();
        InstructionIterator instructions = currentProgram.getListing()
            .getInstructions(true);
        while (instructions.hasNext() && !monitor.isCancelled()) {
            Instruction instruction = instructions.next();
            String mnemonic = instruction.getMnemonicString().toUpperCase();
            if (!(mnemonic.startsWith("F") || mnemonic.startsWith("MOVS"))) {
                continue;
            }
            for (int operand = 0; operand < instruction.getNumOperands(); ++operand) {
                Scalar scalar = instruction.getScalar(operand);
                if (scalar != null && scalar.getUnsignedValue() == 0x7cL) {
                    Function function = currentProgram.getFunctionManager()
                        .getFunctionContaining(instruction.getAddress());
                    if (function != null) {
                        candidates.add(function.getEntryPoint());
                    }
                }
            }
        }
        println("");
        println("FLOATING +0x7C FIELD CANDIDATES");
        for (Address entry : candidates) {
            Function function = currentProgram.getFunctionManager()
                .getFunctionAt(entry);
            println("  function=" + function.getEntryPoint() + " " +
                function.getName(true));
            decompile(decompiler, function,
                "floating +0x7c field candidate", seen);
        }
    }

    @Override
    protected void run() throws Exception {
        String actualSha256 = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(actualSha256)) {
            throw new Exception("Unexpected executable SHA256: " + actualSha256);
        }

        println("SudekiMP first-person presentation report");
        println("SHA256=" + actualSha256);
        println("ImageBase=" + currentProgram.getImageBase());

        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        Set<Address> seen = new HashSet<Address>();
        long[] targets = {
            0x004088a0L, // CCharacterArbiter::IsInFirstPerson
            0x00428950L, // CGamePadControlComponent::SetFirstPersonMode
            0x00429700L, // SetEnableFirstPerson
            0x0042a880L, // SetFirstPersonCameraMode
            0x00429ea0L, // first-person transition helper
            0x004dc330L, // arbiter first-person bit writer
            0x004e0640L, // CSimpleGameModelInterface::Enable
            0x004e12e0L, // CSimpleGameModelInterface::SetROFlags
            0x004e1330L, // CSimpleGameModelInterface::ClrROFlags
            0x004d7e30L, // CCharacterWeapon::SetWeaponVisible
            0x004c1270L, // GetCharacterNumberStat
            0x004ae430L, // first-person group presentation refresh
            0x004dc610L, // enter ranged firing presentation
            0x004dc720L, // leave ranged firing presentation
            0x005888f0L, // ranged first-person model helper
            0x005a5690L, // character model/weapon visibility helper
            0x005113f0L, // ranged firing presentation helper A
            0x005117d0L, // ranged firing presentation helper B
            0x00411680L, // render-object mode helper A
            0x0041ad00L, // render-object mode helper B
            0x004dba40L, // ranged firing eligibility helper
            0x00588a90L, // first/third-person model render-object switch
            0x00511b30L  // entity model attachment switch
        };
        String[] roles = {
            "CCharacterArbiter::IsInFirstPerson",
            "CGamePadControlComponent::SetFirstPersonMode",
            "SetEnableFirstPerson",
            "SetFirstPersonCameraMode",
            "first-person transition helper",
            "arbiter first-person bit writer",
            "CSimpleGameModelInterface::Enable",
            "CSimpleGameModelInterface::SetROFlags",
            "CSimpleGameModelInterface::ClrROFlags",
            "CCharacterWeapon::SetWeaponVisible",
            "GetCharacterNumberStat",
            "first-person group presentation refresh",
            "enter ranged firing presentation",
            "leave ranged firing presentation",
            "ranged first-person model helper",
            "character model/weapon visibility helper",
            "ranged firing presentation helper A",
            "ranged firing presentation helper B",
            "render-object mode helper A",
            "render-object mode helper B",
            "ranged firing eligibility helper",
            "first/third-person model render-object switch",
            "entity model attachment switch"
        };
        for (int index = 0; index < targets.length; ++index) {
            Function function = currentProgram.getFunctionManager()
                .getFunctionAt(address(targets[index]));
            decompile(decompiler, function, roles[index], seen);
            reportCallers(decompiler, function, roles[index], seen);
        }
        reportFirstPersonSymbols();
        reportDataUsers(decompiler, 0x006c1fa0L,
            "FirstPersonModelVisiblityDistance A", seen);
        reportDataUsers(decompiler, 0x006c4324L,
            "FirstPersonModelVisiblityDistance B", seen);
        reportDataUsers(decompiler, 0x00725740L,
            "character-number-stat name table base", seen);
        reportDataUsers(decompiler, 0x00725784L,
            "FirstPersonModelVisiblityDistance table entry", seen);
        reportArbiterFirstPersonFieldUsers(decompiler, seen);
        reportVisibilityDistanceCandidates(decompiler, seen);
        decompile(decompiler, currentProgram.getFunctionManager()
            .getFunctionContaining(address(0x00588601L)),
            "crash-site ranged-model update owner", seen);
        decompiler.dispose();
    }
}
