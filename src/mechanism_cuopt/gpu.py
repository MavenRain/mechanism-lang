"""Optional cuOpt worker. No remote service or mechanism compiler is required."""

import importlib
from pathlib import Path

from .model import AuctionError, require
from .optimization import LinearModel
from .settlement import cases


def propose(model: LinearModel, directory: Path, seconds: float = 30.0) -> dict:
    require(type(seconds) in (int, float) and 0 < seconds <= 3600, "solver seconds must be in (0, 3600]")
    try:
        problem_api = importlib.import_module("cuopt.linear_programming.problem")
        settings_api = importlib.import_module("cuopt.linear_programming.solver_settings")
    except ImportError as error:
        raise AuctionError("cuOpt is unavailable; install cuOpt 26.08 on a supported NVIDIA GPU host, or use --backend reference") from error
    require(callable(getattr(problem_api.Problem, "read", None)), "cuOpt must provide Problem.read for LP files (26.08 API)")
    results = {}
    for name, excluded in cases(model):
        problem = problem_api.Problem.read(str(directory / f"{name}.lp"))
        settings = settings_api.SolverSettings()
        settings.set_parameter("mip_relative_gap", 0.0)
        settings.set_parameter("mip_absolute_gap", 0.0)
        settings.set_parameter("time_limit", seconds)
        problem.solve(settings)
        status = problem.Status.name
        require(status == "Optimal", f"cuOpt {name} returned {status}; no settlement authorized")
        variables = list(problem.getVariables())
        names = [variable.VariableName for variable in variables]
        require(len(names) == len(set(names)) and set(names) == set(model.variables), "cuOpt variable names disagree with the model")
        results[name] = {"status": status, "lp_sha256": model.digest(excluded),
                         "objective": float(problem.ObjValue),
                         "variables": {variable.VariableName: float(variable.Value) for variable in variables}}
    return {"version": 1, "backend": "cuopt", "auction_sha256": model.auction.digest, "cases": results}
