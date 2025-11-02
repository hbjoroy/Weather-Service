-- Weather Dashboard User Profiles Schema
-- Database: bjosoft-weather

-- User profiles table
CREATE TABLE IF NOT EXISTS user_profiles (
    user_id VARCHAR(128) PRIMARY KEY,
    profile_data JSONB NOT NULL,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);

-- Index on updated_at for efficient queries
CREATE INDEX IF NOT EXISTS idx_user_profiles_updated_at ON user_profiles(updated_at);

-- Function to update updated_at timestamp
CREATE OR REPLACE FUNCTION update_updated_at_column()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = CURRENT_TIMESTAMP;
    RETURN NEW;
END;
$$ language 'plpgsql';

-- Trigger to automatically update updated_at
CREATE TRIGGER update_user_profiles_updated_at 
    BEFORE UPDATE ON user_profiles 
    FOR EACH ROW 
    EXECUTE FUNCTION update_updated_at_column();

-- Insert default guest profile (for reference, not actually used)
INSERT INTO user_profiles (user_id, profile_data)
VALUES ('', '{"name": "Guest", "tempUnit": "celsius", "windUnit": "ms", "defaultLocation": "Paros", "isAuthenticated": false}')
ON CONFLICT (user_id) DO NOTHING;

-- Example user profile
INSERT INTO user_profiles (user_id, profile_data)
VALUES ('demo-user', '{"name": "Demo User", "tempUnit": "celsius", "windUnit": "ms", "defaultLocation": "Oslo", "isAuthenticated": true}')
ON CONFLICT (user_id) DO NOTHING;
