import Base
import ./surface_equality.bend as Equality
import ./surface_prelude_poly.bend as Poly
import ./surface_prelude_dependent.bend as Dependent
import ./surface_prelude_equality_ops.bend as Ops
import ./surface_prelude_congruence.bend as Congruence
import ./surface_prelude_transport.bend as Transport
import ./surface_family_groups.bend as Groups
import ./surface_sl.bend as SL
import ./surface_shape_metadata.bend as Veil
import ./surface_family_members.bend as Members
import ./surface_prenex.bend as Prenex
import ./surface_prenex_families.bend as PrenexFamilies
import ./surface_prenex_groups.bend as PrenexGroups
import ./surface_prenex_exports.bend as PrenexExports
import ./surface_prenex_dependency_exports.bend as DependencyExports
import ./surface_template_reuse.bend as Reuse
import ./surface_template_symbolic_reuse.bend as SymbolicReuse
import ./surface_template_composition.bend as Composition
import ./surface_family_poly.bend as FamilyPoly
import ./surface_numbers.bend as Numbers

import ./surface_expectations.bend as Expectations
import ./surface_check_driver.bend as CheckDriver

def arguments(args: List<String>) -> IO(Unit):
  match args:
    case Con{"equality", rest}: Equality.main()
    case Con{"poly", rest}: Poly.arguments(rest)
    case Con{"dependent", rest}: Dependent.arguments(rest)
    case Con{"equality-ops", rest}: Ops.arguments(rest)
    case Con{"congruence", rest}: Congruence.arguments(rest)
    case Con{"transport", rest}: Transport.arguments(rest)
    case Con{"groups", rest}: Groups.arguments(rest)
    case Con{"sl", rest}: SL.arguments(rest)
    case Con{"veil", rest}: Veil.arguments(rest)
    case Con{"members", rest}: Members.arguments(rest)
    case Con{"prenex", rest}: Prenex.arguments(rest)
    case Con{"prenex-families", rest}: PrenexFamilies.arguments(rest)
    case Con{"prenex-groups", rest}: PrenexGroups.arguments(rest)
    case Con{"prenex-exports", rest}: PrenexExports.arguments(rest)
    case Con{"dependency-exports", rest}: DependencyExports.arguments(rest)
    case Con{"reuse", rest}: Reuse.arguments(rest)
    case Con{"symbolic-reuse", rest}: SymbolicReuse.arguments(rest)
    case Con{"composition", rest}: Composition.arguments(rest)
    case Con{"family-poly", rest}: FamilyPoly.arguments(rest)
    case Con{"numbers", rest}: Numbers.arguments(rest)
    case Con{"expectations", rest}: Expectations.arguments(rest)
    case Con{"surface-check", rest}: CheckDriver.run(rest)
    case other: IO.die(Unit, 2, "expected foundation protocol mode")

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    arguments(args)
