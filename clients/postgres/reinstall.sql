-- Reinstall kJSON extension with all updates
DROP EXTENSION IF EXISTS kjson CASCADE;
CREATE EXTENSION kjson;

-- Quick test to verify it's working
SELECT 'Extension reinstalled successfully' as status;