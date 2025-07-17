#!/usr/bin/env python3
"""Run all tests for kJSON Python client."""

import sys
import unittest
import os

# Add the current directory to Python path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

# Import all test modules
from tests import test_types, test_parser, test_serializer

loader = unittest.TestLoader()
suite = unittest.TestSuite()

# Add all tests
suite.addTests(loader.loadTestsFromModule(test_types))
suite.addTests(loader.loadTestsFromModule(test_parser))
suite.addTests(loader.loadTestsFromModule(test_serializer))

# Run tests
if __name__ == "__main__":
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    
    # Exit with error code if tests failed
    sys.exit(0 if result.wasSuccessful() else 1)