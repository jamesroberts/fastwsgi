from click.testing import CliRunner
from pkg_resources import get_distribution

from fastwsgi import run_from_cli


def call_fastwsgi(fnc=None, parameters=None, arguments=None, envs=None):
    fnc = fnc or run_from_cli
    runner = CliRunner()
    envs = envs or {}
    arguments = arguments or []
    parameters = parameters or []
    # catch exceptions enables debugger
    return runner.invoke(fnc, args=arguments + parameters , env=envs, catch_exceptions=False)


class TestCLI:
    def test_help(self):
        result = call_fastwsgi(parameters=["--help"])
        assert result.exit_code == 0
        assert "Run FastWSGI server from CLI" in result.output

    def test_version(self):
        result = call_fastwsgi(parameters=["--version"])
        assert result.exit_code == 0
        assert result.output.strip() == get_distribution("fastwsgi").version

    def test_run_from_cli_invalid_module(self):
        result = call_fastwsgi(arguments=["module:wrong"])
        assert result.exit_code == 1
        assert result.output.strip() == "Error importing WSGI app: No module named 'module'"