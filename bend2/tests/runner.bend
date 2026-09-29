import Base
import ./kernel_numbers.bend as KernelNumbers
import ./kernel_eval.bend as KernelEval
import ./kernel_quantity.bend as KernelQuantity
import ./kernel_check.bend as KernelCheck
import ./kernel_declarations.bend as KernelDeclarations
import ./kernel_conversion.bend as KernelConversion
import ./kernel_recursive.bend as KernelRecursive
import ./kernel_budget_dag.bend as KernelBudgetDag
import ./kernel_budget_callbacks.bend as KernelBudgetCallbacks
import ./kernel_budget_callback_check.bend as KernelBudgetCallbackCheck
import ./kernel_spec_count.bend as KernelSpecCount
import ./levels_exact.bend as LevelsExact
import ./levels_budget.bend as LevelsBudget
import ./levels_checker.bend as LevelsChecker
import ./levels_regression.bend as LevelsRegression
import ./surface_lexer.bend as SurfaceLexer
import ./surface_parser.bend as SurfaceParser
import ./surface_transform.bend as SurfaceTransform
import ./surface_poly.bend as SurfacePoly
import ./surface_family.bend as SurfaceFamily
import ./surface_elab.bend as SurfaceElab
import ./surface_catalog.bend as SurfaceCatalog
import ./surface_program.bend as SurfaceProgram
import ./surface_prelude.bend as SurfacePrelude
import ./surface_crypto.bend as SurfaceCrypto
import ./surface_trust.bend as SurfaceTrust
import ./surface_boundary.bend as SurfaceBoundary
import ./surface_universes.bend as SurfaceUniverses
import ./surface_diagnostics.bend as SurfaceDiagnostics
import ./surface_shape_metadata.bend as SurfaceShapeMetadata
import ./wasm_circuit.bend as WasmCircuit
import ./wasm_order.bend as WasmOrder
import ./wasm_link.bend as WasmLink
import ./wasm_host.bend as WasmHost
import ./wasm_encode.bend as WasmEncode
import ./wasm_emit.bend as WasmEmit
import ./wasm_nat_runtime.bend as WasmNat
import ./wasm_reactor.bend as WasmReactor
import ./wasm_layout.bend as WasmLayout
import ./wasm_oracle.bend as WasmOracle
import ./import_json.bend as ImportJson
import ./import_export.bend as ImportExport
import ./import_translate.bend as ImportTranslate
import ./import_pipeline.bend as ImportPipeline
import ./surface_driver.bend as SurfaceParserDriver
import ./surface_check_driver.bend as SurfaceCheckDriver
import ./kernel_erase_driver.bend as EraseDriver
import ./io_driver.bend as IoDriver
import ./prelude_runtime.bend as PreludeRuntime
import ./wasm_protocol.bend as WasmProtocol
import ./prelude_protocol.bend as PreludeProtocol
import ./mapping_protocol.bend as MappingProtocol

def boolean(result: Bool, name: String) -> IO(Unit):
  match result:
    case True{}:
      IO.print("PASS " ++ name)
    case False{}:
      IO.die(Unit, 1, "FAIL " ++ name)

def unit(name: String) -> IO(Unit):
  match name:
    case "kernel-numbers":
      KernelNumbers.main()
    case "kernel-eval":
      KernelEval.main()
    case "kernel-quantity":
      KernelQuantity.main()
    case "kernel-check":
      KernelCheck.main()
    case "kernel-declarations":
      KernelDeclarations.main()
    case "kernel-conversion":
      KernelConversion.main()
    case "kernel-recursive":
      KernelRecursive.main()
    case "kernel-budget-dag":
      KernelBudgetDag.main()
    case "kernel-budget-callbacks":
      KernelBudgetCallbacks.main()
    case "kernel-budget-callback-check":
      KernelBudgetCallbackCheck.main()
    case "kernel-spec-count":
      KernelSpecCount.main()
    case "levels-exact":
      LevelsExact.main()
    case "levels-budget":
      LevelsBudget.main()
    case "levels-checker":
      LevelsChecker.main()
    case "levels-regression":
      LevelsRegression.main()
    case "surface-lexer":
      boolean(SurfaceLexer.main(), "surface_lexer")
    case "surface-diagnostics":
      boolean(SurfaceDiagnostics.main(), "surface_diagnostics")
    case "surface-shape-metadata":
      SurfaceShapeMetadata.main()
    case "surface-parser":
      boolean(SurfaceParser.main(), "surface_parser")
    case "surface-transform":
      boolean(SurfaceTransform.main(), "surface_transform")
    case "surface-poly":
      boolean(SurfacePoly.main(), "surface_poly")
    case "surface-family":
      boolean(SurfaceFamily.main(), "surface_family")
    case "surface-elab":
      SurfaceElab.main()
    case "surface-catalog":
      SurfaceCatalog.main()
    case "surface-program":
      SurfaceProgram.main()
    case "surface-prelude":
      SurfacePrelude.main()
    case "surface-crypto":
      SurfaceCrypto.main()
    case "surface-trust":
      SurfaceTrust.main()
    case "surface-boundary":
      SurfaceBoundary.main()
    case "surface-universes":
      SurfaceUniverses.main()
    case "wasm-circuit":
      WasmCircuit.main()
    case "wasm-order":
      WasmOrder.main()
    case "wasm-link":
      WasmLink.main()
    case "wasm-host":
      WasmHost.main()
    case name:
      IO.die(Unit, 64, "unknown unit: " ++ name)

def fixture(name: String) -> IO(Unit):
  match name:
    case "encode":
      WasmEncode.main()
    case "emit":
      WasmEmit.main()
    case "nat":
      WasmNat.main()
    case "reactor":
      WasmReactor.main()
    case "layout":
      WasmLayout.main()
    case "oracle":
      WasmOracle.main()
    case name:
      IO.die(Unit, 64, "unknown fixture: " ++ name)

def named(is_fixture: Bool, args: List<String>) -> IO(Unit):
  match is_fixture args:
    case True{} Con{name, Nil{}}:
      fixture(name)
    case False{} Con{name, Nil{}}:
      unit(name)
    case _ _:
      IO.die(Unit, 64, "expected one fixture or unit name")

def dispatch(mode: String, args: List<String>) -> IO(Unit):
  match mode:
    case "unit":
      named(False{}, args)
    case "fixture":
      named(True{}, args)
    case "import-json":
      ImportJson.arguments(args)
    case "import-export":
      ImportExport.arguments(args)
    case "import-translate":
      ImportTranslate.arguments(args)
    case "import-pipeline":
      ImportPipeline.arguments(args)
    case "surface-parser":
      SurfaceParserDriver.run(args)
    case "surface-check":
      SurfaceCheckDriver.run(args)
    case "erase":
      EraseDriver.run(args)
    case "io":
      IoDriver.arguments(args)
    case "prelude-runtime":
      PreludeRuntime.arguments(args)
    case "wasm-protocol":
      WasmProtocol.arguments(args)
    case "prelude-protocol":
      PreludeProtocol.arguments(args)
    case "mapping-protocol":
      MappingProtocol.arguments(args)
    case mode:
      IO.die(Unit, 64, "unknown test mode: " ++ mode)

def arguments(args: List<String>) -> IO(Unit):
  match args:
    case Con{mode, rest}:
      dispatch(mode, rest)
    case Nil{}:
      IO.die(Unit, 64, "expected a test mode")

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    arguments(args)
