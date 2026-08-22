// Exact-build, read-only report for Cafu's testroom weapon-model crash.
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

import java.util.HashSet;
import java.util.Set;

public class CafuWeaponCrashReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private Address address(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
    }

    private Function function(long value) {
        Function exact = currentProgram.getFunctionManager()
            .getFunctionAt(address(value));
        return exact != null ? exact : currentProgram.getFunctionManager()
            .getFunctionContaining(address(value));
    }

    private void decompile(DecompInterface decompiler, long value,
            String role, Set<Address> seen) {
        Function target = function(value);
        println("");
        println("===== " + role + " target=" + address(value) +
            " function=" + (target == null ? "<none>" :
                target.getEntryPoint() + " " + target.getName(true)) +
            " =====");
        if (target == null || !seen.add(target.getEntryPoint())) {
            return;
        }
        DecompileResults result = decompiler.decompileFunction(
            target, 120, monitor);
        println(result.decompileCompleted() ?
            result.getDecompiledFunction().getC() :
            "Decompiler failed: " + result.getErrorMessage());
    }

    private void callers(DecompInterface decompiler, long value,
            String role, Set<Address> seen) {
        Function target = function(value);
        if (target == null) {
            return;
        }
        println("");
        println("CALLERS role=" + role + " function=" +
            target.getEntryPoint());
        ReferenceIterator references = currentProgram.getReferenceManager()
            .getReferencesTo(target.getEntryPoint());
        while (references.hasNext() && !monitor.isCancelled()) {
            Reference reference = references.next();
            if (!reference.getReferenceType().isCall()) {
                continue;
            }
            Function caller = currentProgram.getFunctionManager()
                .getFunctionContaining(reference.getFromAddress());
            println("  callsite=" + reference.getFromAddress() +
                " caller=" + (caller == null ? "<none>" :
                    caller.getEntryPoint() + " " + caller.getName(true)));
            if (caller != null) {
                decompile(decompiler, caller.getEntryPoint().getOffset(),
                    "caller of " + role, seen);
            }
        }
    }

    private void scalarUses(long wanted, String role) {
        println("");
        println("SCALAR_USES role=" + role + " value=0x" +
            Long.toHexString(wanted));
        InstructionIterator iterator = currentProgram.getListing()
            .getInstructions(true);
        while (iterator.hasNext() && !monitor.isCancelled()) {
            Instruction instruction = iterator.next();
            for (int operand = 0; operand < instruction.getNumOperands();
                    ++operand) {
                for (Object object : instruction.getOpObjects(operand)) {
                    if (object instanceof Scalar &&
                        ((Scalar)object).getUnsignedValue() == wanted) {
                        Function owner = currentProgram.getFunctionManager()
                            .getFunctionContaining(instruction.getAddress());
                        println("  " + instruction.getAddress() + " " +
                            instruction + " owner=" +
                            (owner == null ? "<none>" :
                                owner.getEntryPoint() + " " +
                                owner.getName(true)));
                    }
                }
            }
        }
    }

    @Override
    protected void run() throws Exception {
        String actualSha256 = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(actualSha256)) {
            throw new Exception("Unexpected executable SHA256: " + actualSha256);
        }
        println("SudekiMP Cafu weapon crash report");
        println("SHA256=" + actualSha256);
        println("ImageBase=" + currentProgram.getImageBase());

        long[] targets = {
            0x00523500L,
            0x004d7880L,
            0x004d73c0L,
            0x004d7c10L,
            0x004d8790L,
            0x00411730L,
            0x0043e490L,
            0x005d6460L,
            0x005b9760L
        };
        String[] roles = {
            "crashing model-wrapper constructor",
            "weapon-model wrapper allocator",
            "item weapon-model resource selector",
            "CCharacterWeapon item transition",
            "CCharacterWeapon SetWeapon",
            "typed resource lookup adapter",
            "resource manager lookup",
            "render-model base constructor",
            "resource reference release"
        };
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        Set<Address> seen = new HashSet<Address>();
        for (int index = 0; index < targets.length; ++index) {
            decompile(decompiler, targets[index], roles[index], seen);
            callers(decompiler, targets[index], roles[index], seen);
        }
        scalarUses(0x1e4d4e41L, "crashing interface hash");
        scalarUses(0x29L, "required resource type");
        decompiler.dispose();
    }
}
