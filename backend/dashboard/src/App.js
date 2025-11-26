import React, { useState, useEffect } from 'react';
import {
  Container,
  AppBar,
  Toolbar,
  Typography,
  Grid,
  Card,
  CardContent,
  Box,
  Chip,
  Alert,
  CircularProgress,
} from '@mui/material';
import {
  Favorite,
  Thermostat,
  VolumeUp,
  Warning,
  CheckCircle,
} from '@mui/icons-material';
import {
  LineChart,
  Line,
  XAxis,
  YAxis,
  CartesianGrid,
  Tooltip,
  Legend,
  ResponsiveContainer,
} from 'recharts';
import axios from 'axios';
import { format } from 'date-fns';
import './App.css';

const API_URL = process.env.REACT_APP_API_URL || 'http://localhost:5000';

function App() {
  const [devices, setDevices] = useState([]);
  const [selectedDevice, setSelectedDevice] = useState('ESP32-001');
  const [latestVitals, setLatestVitals] = useState(null);
  const [realtimeData, setRealtimeData] = useState([]);
  const [fallAlerts, setFallAlerts] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);

  // Fetch devices
  useEffect(() => {
    fetchDevices();
    const interval = setInterval(fetchDevices, 60000); // Update every minute
    return () => clearInterval(interval);
  }, []);

  // Fetch data for selected device
  useEffect(() => {
    if (selectedDevice) {
      fetchData();
      const interval = setInterval(fetchData, 5000); // Update every 5 seconds
      return () => clearInterval(interval);
    }
  }, [selectedDevice]);

  const fetchDevices = async () => {
    try {
      const response = await axios.get(`${API_URL}/api/devices`);
      setDevices(response.data);
    } catch (err) {
      console.error('Error fetching devices:', err);
    }
  };

  const fetchData = async () => {
    try {
      setLoading(true);
      setError(null);

      // Fetch latest vitals
      const vitalsRes = await axios.get(`${API_URL}/api/vitals/latest`);
      const deviceVitals = vitalsRes.data.find(v => v.device_id === selectedDevice);
      setLatestVitals(deviceVitals);

      // Fetch real-time data history (last 50 readings)
      const realtimeRes = await axios.get(`${API_URL}/api/realtime/${selectedDevice}?limit=50`);
      // Convert numeric strings to numbers for charts
      const processedData = realtimeRes.data.map(item => ({
        ...item,
        body_temperature: parseFloat(item.body_temperature),
        ambient_temperature: parseFloat(item.ambient_temperature),
        heart_rate: parseInt(item.heart_rate),
        noise_level: parseInt(item.noise_level)
      }));
      setRealtimeData(processedData.reverse());

      // Fetch pending fall alerts
      const fallsRes = await axios.get(`${API_URL}/api/falls/alerts/pending`);
      setFallAlerts(fallsRes.data);

      setLoading(false);
    } catch (err) {
      console.error('Error fetching data:', err);
      setError('Failed to fetch data. Please check if the API server is running.');
      setLoading(false);
    }
  };

  const getFallStateText = (state) => {
    const states = ['Normal', 'Warning', 'Fall Detected', 'Dangerous', 'Recovery'];
    return states[state] || 'Unknown';
  };

  const getFallStateColor = (state) => {
    const colors = ['success', 'warning', 'error', 'error', 'info'];
    return colors[state] || 'default';
  };

  if (loading && !latestVitals) {
    return (
      <Box display="flex" justifyContent="center" alignItems="center" minHeight="100vh">
        <CircularProgress />
      </Box>
    );
  }

  return (
    <div className="App">
      <AppBar position="static">
        <Toolbar>
          <Typography variant="h6" component="div" sx={{ flexGrow: 1 }}>
            🏥 Health Monitoring Dashboard
          </Typography>
          <Typography variant="body2">
            Device: {selectedDevice}
          </Typography>
        </Toolbar>
      </AppBar>

      <Container maxWidth="xl" sx={{ mt: 4, mb: 4 }}>
        {error && (
          <Alert severity="error" sx={{ mb: 2 }}>
            {error}
          </Alert>
        )}

        {/* Fall Alerts */}
        {fallAlerts.length > 0 && (
          <Alert severity="error" icon={<Warning />} sx={{ mb: 3 }}>
            <Typography variant="h6">🚨 FALL ALERT!</Typography>
            {fallAlerts.map(alert => (
              <Typography key={alert.id} variant="body2">
                Fall detected {Math.floor(alert.seconds_ago)} seconds ago - 
                Jerk: {alert.jerk_magnitude.toFixed(0)} m/s³, 
                SVM: {alert.svm_value.toFixed(2)}g
              </Typography>
            ))}
          </Alert>
        )}

        {/* Vital Signs Cards */}
        <Grid container spacing={3} sx={{ mb: 3 }}>
          {/* Heart Rate */}
          <Grid item xs={12} sm={6} md={3}>
            <Card>
              <CardContent>
                <Box display="flex" alignItems="center" justifyContent="space-between">
                  <Favorite color="error" sx={{ fontSize: 40 }} />
                  <Box textAlign="right">
                    <Typography color="textSecondary" variant="caption">
                      Heart Rate
                    </Typography>
                    <Typography variant="h4">
                      {latestVitals?.heart_rate || '--'}
                    </Typography>
                    <Typography variant="body2">BPM</Typography>
                    {latestVitals?.alert_hr_abnormal && (
                      <Chip label="Abnormal" color="warning" size="small" />
                    )}
                  </Box>
                </Box>
              </CardContent>
            </Card>
          </Grid>

          {/* Body Temperature */}
          <Grid item xs={12} sm={6} md={3}>
            <Card>
              <CardContent>
                <Box display="flex" alignItems="center" justifyContent="space-between">
                  <Thermostat color="primary" sx={{ fontSize: 40 }} />
                  <Box textAlign="right">
                    <Typography color="textSecondary" variant="caption">
                      Body Temp
                    </Typography>
                    <Typography variant="h4">
                      {latestVitals?.body_temperature ? parseFloat(latestVitals.body_temperature).toFixed(1) : '--'}
                    </Typography>
                    <Typography variant="body2">°C</Typography>
                    {latestVitals?.alert_temp_abnormal && (
                      <Chip label="Fever" color="error" size="small" />
                    )}
                  </Box>
                </Box>
              </CardContent>
            </Card>
          </Grid>

          {/* Noise Level */}
          <Grid item xs={12} sm={6} md={3}>
            <Card>
              <CardContent>
                <Box display="flex" alignItems="center" justifyContent="space-between">
                  <VolumeUp color="action" sx={{ fontSize: 40 }} />
                  <Box textAlign="right">
                    <Typography color="textSecondary" variant="caption">
                      Noise Level
                    </Typography>
                    <Typography variant="h4">
                      {latestVitals?.noise_level || '--'}
                    </Typography>
                    <Typography variant="body2">dB</Typography>
                    {latestVitals?.alert_noise && (
                      <Chip label="Too Loud" color="warning" size="small" />
                    )}
                  </Box>
                </Box>
              </CardContent>
            </Card>
          </Grid>

          {/* Fall Status */}
          <Grid item xs={12} sm={6} md={3}>
            <Card>
              <CardContent>
                <Box display="flex" alignItems="center" justifyContent="space-between">
                  {latestVitals?.fall_state === 0 ? (
                    <CheckCircle color="success" sx={{ fontSize: 40 }} />
                  ) : (
                    <Warning color="error" sx={{ fontSize: 40 }} />
                  )}
                  <Box textAlign="right">
                    <Typography color="textSecondary" variant="caption">
                      Fall Status
                    </Typography>
                    <Typography variant="h6">
                      {getFallStateText(latestVitals?.fall_state)}
                    </Typography>
                    <Chip 
                      label={getFallStateText(latestVitals?.fall_state)}
                      color={getFallStateColor(latestVitals?.fall_state)}
                      size="small"
                    />
                  </Box>
                </Box>
              </CardContent>
            </Card>
          </Grid>
        </Grid>

        {/* Charts */}
        <Grid container spacing={3}>
          {/* Heart Rate Chart */}
          <Grid item xs={12} md={6}>
            <Card>
              <CardContent>
                <Typography variant="h6" gutterBottom>
                  Heart Rate History
                </Typography>
                <ResponsiveContainer width="100%" height={300}>
                  <LineChart data={realtimeData}>
                    <CartesianGrid strokeDasharray="3 3" />
                    <XAxis 
                      dataKey="timestamp" 
                      tickFormatter={(time) => format(new Date(time), 'HH:mm:ss')}
                    />
                    <YAxis domain={[40, 150]} />
                    <Tooltip 
                      labelFormatter={(time) => format(new Date(time), 'HH:mm:ss')}
                    />
                    <Legend />
                    <Line 
                      type="monotone" 
                      dataKey="heart_rate" 
                      stroke="#f44336" 
                      name="Heart Rate (BPM)"
                      strokeWidth={2}
                    />
                  </LineChart>
                </ResponsiveContainer>
              </CardContent>
            </Card>
          </Grid>

          {/* Temperature Chart */}
          <Grid item xs={12} md={6}>
            <Card>
              <CardContent>
                <Typography variant="h6" gutterBottom>
                  Temperature History
                </Typography>
                <ResponsiveContainer width="100%" height={300}>
                  <LineChart data={realtimeData}>
                    <CartesianGrid strokeDasharray="3 3" />
                    <XAxis 
                      dataKey="timestamp" 
                      tickFormatter={(time) => format(new Date(time), 'HH:mm:ss')}
                    />
                    <YAxis domain={[35, 40]} />
                    <Tooltip 
                      labelFormatter={(time) => format(new Date(time), 'HH:mm:ss')}
                    />
                    <Legend />
                    <Line 
                      type="monotone" 
                      dataKey="body_temperature" 
                      stroke="#2196f3" 
                      name="Body Temp (°C)"
                      strokeWidth={2}
                    />
                    <Line 
                      type="monotone" 
                      dataKey="ambient_temperature" 
                      stroke="#ff9800" 
                      name="Ambient Temp (°C)"
                      strokeWidth={2}
                    />
                  </LineChart>
                </ResponsiveContainer>
              </CardContent>
            </Card>
          </Grid>

          {/* Noise Level Chart */}
          <Grid item xs={12}>
            <Card>
              <CardContent>
                <Typography variant="h6" gutterBottom>
                  Noise Level History
                </Typography>
                <ResponsiveContainer width="100%" height={300}>
                  <LineChart data={realtimeData}>
                    <CartesianGrid strokeDasharray="3 3" />
                    <XAxis 
                      dataKey="timestamp" 
                      tickFormatter={(time) => format(new Date(time), 'HH:mm:ss')}
                    />
                    <YAxis domain={[0, 120]} />
                    <Tooltip 
                      labelFormatter={(time) => format(new Date(time), 'HH:mm:ss')}
                    />
                    <Legend />
                    <Line 
                      type="monotone" 
                      dataKey="noise_level" 
                      stroke="#9c27b0" 
                      name="Noise Level (dB)"
                      strokeWidth={2}
                    />
                  </LineChart>
                </ResponsiveContainer>
              </CardContent>
            </Card>
          </Grid>
        </Grid>

        {/* Last Updated */}
        <Box mt={3} textAlign="center">
          <Typography variant="body2" color="textSecondary">
            Last updated: {latestVitals?.timestamp ? format(new Date(latestVitals.timestamp), 'PPpp') : 'N/A'}
          </Typography>
        </Box>
      </Container>
    </div>
  );
}

export default App;
