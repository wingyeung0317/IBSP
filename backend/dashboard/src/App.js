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
  Button,
  IconButton,
  Snackbar,
  Tabs,
  Tab,
  Table,
  TableBody,
  TableCell,
  TableContainer,
  TableHead,
  TableRow,
  TablePagination,
  Paper,
} from '@mui/material';
import {
  Favorite,
  Thermostat,
  VolumeUp,
  Warning,
  CheckCircle,
  Refresh,
  RestartAlt,
  Article,
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
  const [ecgData, setEcgData] = useState(null);
  const [fallAlerts, setFallAlerts] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);
  const [snackbarOpen, setSnackbarOpen] = useState(false);
  const [snackbarMessage, setSnackbarMessage] = useState('');
  const [currentTab, setCurrentTab] = useState('dashboard');
  const [logs, setLogs] = useState([]);
  const [logsTotal, setLogsTotal] = useState(0);
  const [logsPage, setLogsPage] = useState(0);
  const [logsLoading, setLogsLoading] = useState(false);
  const [alarmSound] = useState(() => {
    const audio = new Audio('data:audio/wav;base64,UklGRnoGAABXQVZFZm10IBAAAAABAAEAQB8AAEAfAAABAAgAZGF0YQoGAACBhYqFbF1fdJivrJBhNjVgodDbq2EcBj+a2/LDciUFLIHO8tiJNwgZaLvt559NEAxQp+PwtmMcBjiR1/LMeSwFJHfH8N2QQAoUXrTp66hVFApGn+DyvmwhBzGF0fPTgjMGHm7A7+OZURE=');
    audio.loop = true;
    return audio;
  });
  const [isAlarmPlaying, setIsAlarmPlaying] = useState(false);
  const [emergencyState, setEmergencyState] = useState(null); // 'fall' or 'unconscious'

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

      // Check for emergency conditions
      if (deviceVitals) {
        if (deviceVitals.fall_state === 3) {
          // Unconscious/Dangerous state - HIGHEST PRIORITY
          if (emergencyState !== 'unconscious') {
            setEmergencyState('unconscious');
            playAlarm();
          }
        } else if (deviceVitals.fall_state === 2) {
          // Fall detected
          if (emergencyState !== 'fall' && emergencyState !== 'unconscious') {
            setEmergencyState('fall');
            playAlarm();
          }
        } else if (deviceVitals.fall_state === 0 || deviceVitals.fall_state === 4) {
          // Normal or Recovery - stop alarm
          if (emergencyState) {
            stopAlarm();
          }
        }
      }

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

      // Fetch latest ECG data with PQRST waveform
      const ecgRes = await axios.get(`${API_URL}/api/ecg/${selectedDevice}?limit=1`);
      if (ecgRes.data.length > 0) {
        setEcgData(ecgRes.data[0]);
      }

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

  const playAlarm = () => {
    try {
      if (!isAlarmPlaying) {
        alarmSound.play().catch(err => console.log('Audio play failed:', err));
        setIsAlarmPlaying(true);
      }
    } catch (err) {
      console.error('Failed to play alarm:', err);
    }
  };

  const stopAlarm = () => {
    try {
      alarmSound.pause();
      alarmSound.currentTime = 0;
      setIsAlarmPlaying(false);
      setEmergencyState(null);
    } catch (err) {
      console.error('Failed to stop alarm:', err);
    }
  };

  const handleDismissEmergency = () => {
    stopAlarm();
    setSnackbarMessage('Emergency alert dismissed');
    setSnackbarOpen(true);
  };

  const handleReset = () => {
    // Refresh all data
    fetchData();
    setSnackbarMessage('Dashboard refreshed successfully');
    setSnackbarOpen(true);
  };

  const handleClearHistory = async () => {
    if (!window.confirm('🔄 重置追蹤點？\n\n這將讓儀表板只顯示從現在開始的新數據。\n歷史記錄會保留在資料庫中，但不會在圖表中顯示。')) {
      return;
    }

    try {
      await axios.post(`${API_URL}/api/tracking/reset/${selectedDevice}`);
      setSnackbarMessage('✅ 追蹤點已重置！從現在開始記錄新數據。');
      setSnackbarOpen(true);
      
      // Clear local state
      setRealtimeData([]);
      setEcgData(null);
      setFallAlerts([]);
      
      // Refresh data after a short delay
      setTimeout(() => {
        fetchData();
      }, 500);
      
    } catch (err) {
      console.error('Error resetting tracking:', err);
      setSnackbarMessage('❌ 重置失敗，請重試。');
      setSnackbarOpen(true);
    }
  };

  const handleAcknowledgeAlerts = async () => {
    try {
      // Acknowledge all pending fall alerts
      for (const alert of fallAlerts) {
        await axios.put(`${API_URL}/api/falls/${alert.id}/acknowledge`);
      }
      setFallAlerts([]);
      setSnackbarMessage('All alerts acknowledged');
      setSnackbarOpen(true);
    } catch (err) {
      console.error('Error acknowledging alerts:', err);
      setSnackbarMessage('Failed to acknowledge alerts');
      setSnackbarOpen(true);
    }
  };

  const handleSnackbarClose = () => {
    setSnackbarOpen(false);
  };

  const fetchLogs = async (page = 0) => {
    try {
      setLogsLoading(true);
      const limit = 50;
      const offset = page * limit;
      const response = await axios.get(`${API_URL}/api/logs/${selectedDevice}?limit=${limit}&offset=${offset}`);
      setLogs(response.data.logs);
      setLogsTotal(response.data.total);
      setLogsPage(page);
      setLogsLoading(false);
    } catch (err) {
      console.error('Error fetching logs:', err);
      setLogsLoading(false);
    }
  };

  const handleTabChange = (tab) => {
    setCurrentTab(tab);
    if (tab === 'logs') {
      fetchLogs(0);
    }
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
          <Button 
            color="inherit" 
            onClick={handleClearHistory}
            startIcon={<RestartAlt />}
            sx={{ mr: 2 }}
          >
            Reset Panel
          </Button>
          <IconButton 
            color="inherit" 
            onClick={handleReset}
            title="Refresh Data"
            sx={{ mr: 2 }}
          >
            <Refresh />
          </IconButton>
          <Typography variant="body2">
            Device: {selectedDevice}
          </Typography>
        </Toolbar>
      </AppBar>

      {/* Tab Navigation */}
      <Box sx={{ borderBottom: 1, borderColor: 'divider', bgcolor: 'background.paper' }}>
        <Tabs value={currentTab} onChange={(e, newValue) => handleTabChange(newValue)} centered>
          <Tab label="Dashboard" value="dashboard" />
          <Tab label="Packet Logs" value="logs" icon={<Article />} iconPosition="start" />
        </Tabs>
      </Box>

      <Container maxWidth="xl" sx={{ mt: 4, mb: 4 }}>
        {error && (
          <Alert severity="error" sx={{ mb: 2 }}>
            {error}
          </Alert>
        )}

        {/* Dashboard Tab */}
        {currentTab === 'dashboard' && (
          <>
            {/* EMERGENCY OVERLAY - Full screen alarm for fall/unconscious */}
            {emergencyState && (
              <Box
                sx={{
                  position: 'fixed',
                  top: 0,
                  left: 0,
                  right: 0,
                  bottom: 0,
                  bgcolor: emergencyState === 'unconscious' ? 'rgba(139, 0, 0, 0.95)' : 'rgba(211, 47, 47, 0.95)',
                  zIndex: 9999,
                  display: 'flex',
                  flexDirection: 'column',
                  alignItems: 'center',
                  justifyContent: 'center',
                  animation: 'pulse 1s infinite',
                  '@keyframes pulse': {
                    '0%, 100%': { opacity: 0.95 },
                    '50%': { opacity: 1 },
                  },
                }}
              >
                <Warning sx={{ fontSize: 120, color: 'white', mb: 3, animation: 'shake 0.5s infinite' }} />
                <Typography variant="h1" color="white" fontWeight="bold" sx={{ mb: 2, textShadow: '0 0 20px rgba(255,255,255,0.5)' }}>
                  {emergencyState === 'unconscious' ? '⚠️ UNCONSCIOUS' : '🚨 FALL DETECTED'}
                </Typography>
                <Typography variant="h3" color="white" sx={{ mb: 1 }}>
                  Device: {selectedDevice}
                </Typography>
                <Typography variant="h4" color="white" sx={{ mb: 4, opacity: 0.9 }}>
                  {emergencyState === 'unconscious' 
                    ? 'PATIENT NOT MOVING - IMMEDIATE ATTENTION REQUIRED!' 
                    : 'FALL EVENT DETECTED - CHECK PATIENT STATUS'}
                </Typography>
                
                {latestVitals && (
                  <Box sx={{ bgcolor: 'rgba(255,255,255,0.1)', p: 4, borderRadius: 2, mb: 4 }}>
                    <Grid container spacing={3} sx={{ color: 'white' }}>
                      <Grid item xs={4}>
                        <Typography variant="h6">Heart Rate</Typography>
                        <Typography variant="h3">{latestVitals.heart_rate} BPM</Typography>
                      </Grid>
                      <Grid item xs={4}>
                        <Typography variant="h6">Body Temp</Typography>
                        <Typography variant="h3">{parseFloat(latestVitals.body_temperature).toFixed(1)}°C</Typography>
                      </Grid>
                      <Grid item xs={4}>
                        <Typography variant="h6">Fall State</Typography>
                        <Typography variant="h3">{getFallStateText(latestVitals.fall_state)}</Typography>
                      </Grid>
                    </Grid>
                  </Box>
                )}

                <Box sx={{ display: 'flex', gap: 2 }}>
                  <Button 
                    variant="contained" 
                    size="large"
                    onClick={handleDismissEmergency}
                    sx={{ 
                      bgcolor: 'white', 
                      color: 'error.main',
                      fontSize: '1.5rem',
                      px: 6,
                      py: 2,
                      '&:hover': { bgcolor: 'rgba(255,255,255,0.9)' },
                    }}
                  >
                    DISMISS ALARM
                  </Button>
                  <Button 
                    variant="outlined" 
                    size="large"
                    onClick={handleAcknowledgeAlerts}
                    sx={{ 
                      borderColor: 'white',
                      color: 'white',
                      fontSize: '1.5rem',
                      px: 6,
                      py: 2,
                      '&:hover': { borderColor: 'white', bgcolor: 'rgba(255,255,255,0.1)' },
                    }}
                  >
                    ACKNOWLEDGE
                  </Button>
                </Box>
              </Box>
            )}

            {/* Fall Alerts Banner (shown when not in emergency mode) */}
            {fallAlerts.length > 0 && !emergencyState && (
          <Alert 
            severity="error" 
            icon={<Warning />} 
            sx={{ mb: 3 }}
            action={
              <Button 
                color="inherit" 
                size="small" 
                onClick={handleAcknowledgeAlerts}
                startIcon={<CheckCircle />}
              >
                Acknowledge
              </Button>
            }
          >
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
            <Card 
              sx={{ 
                border: (latestVitals?.fall_state >= 2) ? '3px solid' : 'none',
                borderColor: latestVitals?.fall_state === 3 ? 'error.dark' : 'error.main',
                bgcolor: latestVitals?.fall_state === 3 ? 'error.50' : latestVitals?.fall_state === 2 ? 'warning.50' : 'inherit',
                animation: (latestVitals?.fall_state >= 2) ? 'borderPulse 1s infinite' : 'none',
                '@keyframes borderPulse': {
                  '0%, 100%': { boxShadow: '0 0 0 0 rgba(211, 47, 47, 0.7)' },
                  '50%': { boxShadow: '0 0 0 10px rgba(211, 47, 47, 0)' },
                },
              }}
            >
              <CardContent>
                <Box display="flex" alignItems="center" justifyContent="space-between">
                  {latestVitals?.fall_state === 0 ? (
                    <CheckCircle color="success" sx={{ fontSize: 40 }} />
                  ) : latestVitals?.fall_state === 3 ? (
                    <Warning color="error" sx={{ fontSize: 40, animation: 'shake 0.5s infinite' }} />
                  ) : (
                    <Warning color="error" sx={{ fontSize: 40 }} />
                  )}
                  <Box textAlign="right">
                    <Typography color="textSecondary" variant="caption">
                      Fall Status
                    </Typography>
                    <Typography 
                      variant="h6" 
                      sx={{ 
                        fontWeight: latestVitals?.fall_state >= 2 ? 'bold' : 'normal',
                        color: latestVitals?.fall_state === 3 ? 'error.dark' : 'inherit'
                      }}
                    >
                      {getFallStateText(latestVitals?.fall_state)}
                    </Typography>
                    <Chip 
                      label={getFallStateText(latestVitals?.fall_state)}
                      color={getFallStateColor(latestVitals?.fall_state)}
                      size="small"
                      sx={{ fontWeight: latestVitals?.fall_state >= 2 ? 'bold' : 'normal' }}
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
          <Grid item xs={12} md={6}>
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

          {/* ECG PQRST Spectrum */}
          <Grid item xs={12} md={6}>
            <Card>
              <CardContent>
                <Typography variant="h6" gutterBottom>
                  ECG Waveform (PQRST)
                </Typography>
                {ecgData ? (
                  <>
                    <ResponsiveContainer width="100%" height={300}>
                      <LineChart data={[
                        { label: 'P', amplitude: ecgData.p_amplitude / 1000 },
                        { label: 'Q', amplitude: ecgData.q_amplitude / 1000 },
                        { label: 'R', amplitude: ecgData.r_amplitude / 1000 },
                        { label: 'S', amplitude: ecgData.s_amplitude / 1000 },
                        { label: 'T', amplitude: ecgData.t_amplitude / 1000 }
                      ]}>
                        <CartesianGrid strokeDasharray="3 3" />
                        <XAxis 
                          dataKey="label" 
                          label={{ value: 'PQRST Waves', position: 'insideBottom', offset: -5 }}
                        />
                        <YAxis 
                          label={{ value: 'Amplitude (mV)', angle: -90, position: 'insideLeft' }}
                        />
                        <Tooltip 
                          formatter={(value) => [`${value.toFixed(3)} mV`, 'Amplitude']}
                        />
                        <Legend />
                        <Line 
                          type="monotone" 
                          dataKey="amplitude" 
                          stroke="#d32f2f" 
                          name="ECG Signal"
                          strokeWidth={3}
                          dot={{ r: 6 }}
                        />
                      </LineChart>
                    </ResponsiveContainer>
                    <Box mt={2} display="flex" flexDirection="column" gap={1}>
                      <Box display="flex" justifyContent="space-around">
                        <Chip 
                          label={`P: ${(ecgData.p_amplitude / 1000).toFixed(2)} mV`} 
                          size="small" 
                          color="primary" 
                          variant="outlined" 
                        />
                        <Chip 
                          label={`Q: ${(ecgData.q_amplitude / 1000).toFixed(2)} mV`} 
                          size="small" 
                          color="secondary" 
                          variant="outlined" 
                        />
                        <Chip 
                          label={`R: ${(ecgData.r_amplitude / 1000).toFixed(2)} mV`} 
                          size="small" 
                          color="error" 
                          variant="outlined" 
                        />
                        <Chip 
                          label={`S: ${(ecgData.s_amplitude / 1000).toFixed(2)} mV`} 
                          size="small" 
                          color="warning" 
                          variant="outlined" 
                        />
                        <Chip 
                          label={`T: ${(ecgData.t_amplitude / 1000).toFixed(2)} mV`} 
                          size="small" 
                          color="success" 
                          variant="outlined" 
                        />
                      </Box>
                      <Box display="flex" justifyContent="center" gap={2}>
                        <Chip 
                          label={`QRS Width: ${ecgData.qrs_width} ms`} 
                          size="small" 
                          variant="outlined" 
                        />
                        <Chip 
                          label={`QT Interval: ${ecgData.qt_interval} ms`} 
                          size="small" 
                          variant="outlined" 
                        />
                      </Box>
                      <Typography variant="caption" color="textSecondary" textAlign="center">
                        Last updated: {format(new Date(ecgData.timestamp), 'PPpp')}
                      </Typography>
                    </Box>
                  </>
                ) : (
                  <Box display="flex" justifyContent="center" alignItems="center" height={300}>
                    <Typography variant="body2" color="textSecondary">
                      Waiting for ECG data from device...
                    </Typography>
                  </Box>
                )}
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
          </>
        )}

        {/* Logs Tab */}
        {currentTab === 'logs' && (
          <Card>
            <CardContent>
              <Typography variant="h5" gutterBottom>
                📦 Packet Logs - {selectedDevice}
              </Typography>
              <Typography variant="body2" color="textSecondary" gutterBottom>
                Total packets: {logsTotal} | Showing: {logs.length}
              </Typography>
              
              {logsLoading ? (
                <Box display="flex" justifyContent="center" p={4}>
                  <CircularProgress />
                </Box>
              ) : (
                <>
                  <TableContainer component={Paper} sx={{ mt: 2 }}>
                    <Table size="small">
                      <TableHead>
                        <TableRow>
                          <TableCell><strong>Timestamp</strong></TableCell>
                          <TableCell><strong>Device ID</strong></TableCell>
                          <TableCell align="center"><strong>Type</strong></TableCell>
                          <TableCell align="center"><strong>Frame #</strong></TableCell>
                          <TableCell align="center"><strong>Size</strong></TableCell>
                          <TableCell align="center"><strong>RSSI</strong></TableCell>
                          <TableCell><strong>Raw Data (Hex)</strong></TableCell>
                        </TableRow>
                      </TableHead>
                      <TableBody>
                        {logs.map((log) => (
                          <TableRow 
                            key={log.id}
                            sx={{ 
                              '&:hover': { bgcolor: 'action.hover' },
                              bgcolor: log.packet_type === 3 ? 'error.50' : 'inherit'
                            }}
                          >
                            <TableCell>
                              <Typography variant="caption" sx={{ fontFamily: 'monospace' }}>
                                {format(new Date(log.timestamp), 'MM/dd HH:mm:ss')}
                              </Typography>
                            </TableCell>
                            <TableCell>
                              <Chip label={log.device_id} size="small" variant="outlined" />
                            </TableCell>
                            <TableCell align="center">
                              <Chip 
                                label={
                                  log.packet_type === 1 ? 'Realtime' : 
                                  log.packet_type === 2 ? 'ECG' : 
                                  log.packet_type === 3 ? 'Fall Event' : 
                                  'Unknown'
                                }
                                color={
                                  log.packet_type === 1 ? 'primary' : 
                                  log.packet_type === 2 ? 'info' : 
                                  log.packet_type === 3 ? 'error' : 
                                  'default'
                                }
                                size="small"
                              />
                            </TableCell>
                            <TableCell align="center">
                              <Typography variant="body2" sx={{ fontFamily: 'monospace' }}>
                                {log.frame_counter}
                              </Typography>
                            </TableCell>
                            <TableCell align="center">
                              <Typography variant="caption">
                                {log.data_length} bytes
                              </Typography>
                            </TableCell>
                            <TableCell align="center">
                              <Chip 
                                label={`${log.rssi} dBm`}
                                size="small"
                                color={log.rssi > -70 ? 'success' : log.rssi > -90 ? 'warning' : 'error'}
                                variant="outlined"
                              />
                            </TableCell>
                            <TableCell>
                              <Typography 
                                variant="caption" 
                                sx={{ 
                                  fontFamily: 'monospace',
                                  fontSize: '0.7rem',
                                  wordBreak: 'break-all'
                                }}
                              >
                                {log.raw_data ? (() => {
                                  // Convert Buffer/byte array to hex string in browser
                                  const bytes = log.raw_data.data || log.raw_data;
                                  const hexString = Array.from(bytes)
                                    .map(b => b.toString(16).padStart(2, '0'))
                                    .join('');
                                  return hexString.substring(0, 60) + (hexString.length > 60 ? '...' : '');
                                })() : 'N/A'}
                              </Typography>
                            </TableCell>
                          </TableRow>
                        ))}
                      </TableBody>
                    </Table>
                  </TableContainer>
                  
                  <TablePagination
                    component="div"
                    count={logsTotal}
                    page={logsPage}
                    onPageChange={(e, newPage) => fetchLogs(newPage)}
                    rowsPerPage={50}
                    rowsPerPageOptions={[50]}
                  />
                </>
              )}
            </CardContent>
          </Card>
        )}
      </Container>

      {/* Snackbar for notifications */}
      <Snackbar
        open={snackbarOpen}
        autoHideDuration={3000}
        onClose={handleSnackbarClose}
        message={snackbarMessage}
        anchorOrigin={{ vertical: 'bottom', horizontal: 'center' }}
      />
    </div>
  );
}

export default App;
