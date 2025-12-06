/**
 * Notification Service for Health Monitoring System
 * ================================================
 * 
 * Supports multiple notification channels:
 * - Telegram Bot API
 * - Discord Webhooks
 * - WhatsApp (via Twilio)
 * 
 * This service sends alerts for:
 * - Fall detection events
 * - Heart rate abnormalities
 * - Temperature abnormalities
 * - High noise level alerts
 */

const axios = require('axios');

// Load configuration from environment variables
const config = {
  telegram: {
    enabled: process.env.TELEGRAM_ENABLED === 'true',
    botToken: process.env.TELEGRAM_BOT_TOKEN,
    chatIds: process.env.TELEGRAM_CHAT_IDS ? process.env.TELEGRAM_CHAT_IDS.split(',') : []
  },
  discord: {
    enabled: process.env.DISCORD_ENABLED === 'true',
    webhookUrls: process.env.DISCORD_WEBHOOK_URLS ? process.env.DISCORD_WEBHOOK_URLS.split(',') : []
  },
  whatsapp: {
    enabled: process.env.WHATSAPP_ENABLED === 'true',
    accountSid: process.env.TWILIO_ACCOUNT_SID,
    authToken: process.env.TWILIO_AUTH_TOKEN,
    fromNumber: process.env.TWILIO_WHATSAPP_FROM,
    toNumbers: process.env.WHATSAPP_TO_NUMBERS ? process.env.WHATSAPP_TO_NUMBERS.split(',') : []
  }
};

/**
 * Send notification via Telegram
 */
async function sendTelegramNotification(message) {
  if (!config.telegram.enabled || !config.telegram.botToken) {
    console.log('⚠️  Telegram notifications disabled or not configured');
    return { success: false, reason: 'disabled' };
  }

  const results = [];
  
  for (const chatId of config.telegram.chatIds) {
    try {
      const url = `https://api.telegram.org/bot${config.telegram.botToken}/sendMessage`;
      
      const response = await axios.post(url, {
        chat_id: chatId.trim(),
        text: message,
        parse_mode: 'Markdown',
        disable_web_page_preview: true
      }, {
        timeout: 10000
      });

      if (response.data.ok) {
        console.log(`✅ Telegram notification sent to chat ${chatId}`);
        results.push({ chatId, success: true });
      } else {
        console.error(`❌ Telegram API error for chat ${chatId}:`, response.data);
        results.push({ chatId, success: false, error: response.data });
      }
    } catch (error) {
      console.error(`❌ Failed to send Telegram notification to ${chatId}:`, error.message);
      results.push({ chatId, success: false, error: error.message });
    }
  }

  return {
    success: results.some(r => r.success),
    results
  };
}

/**
 * Send notification via Discord webhook
 */
async function sendDiscordNotification(title, description, color = 0xFF0000, fields = []) {
  if (!config.discord.enabled || config.discord.webhookUrls.length === 0) {
    console.log('⚠️  Discord notifications disabled or not configured');
    return { success: false, reason: 'disabled' };
  }

  const results = [];

  const embed = {
    title,
    description,
    color,
    fields,
    timestamp: new Date().toISOString(),
    footer: {
      text: 'Health Monitoring System'
    }
  };

  for (const webhookUrl of config.discord.webhookUrls) {
    try {
      const response = await axios.post(webhookUrl.trim(), {
        embeds: [embed]
      }, {
        timeout: 10000
      });

      if (response.status === 204) {
        console.log(`✅ Discord notification sent to webhook`);
        results.push({ webhook: webhookUrl.substring(0, 50) + '...', success: true });
      } else {
        console.error(`❌ Discord webhook error:`, response.status);
        results.push({ webhook: webhookUrl.substring(0, 50) + '...', success: false, status: response.status });
      }
    } catch (error) {
      console.error(`❌ Failed to send Discord notification:`, error.message);
      results.push({ webhook: webhookUrl.substring(0, 50) + '...', success: false, error: error.message });
    }
  }

  return {
    success: results.some(r => r.success),
    results
  };
}

/**
 * Send notification via WhatsApp (using Twilio)
 */
async function sendWhatsAppNotification(message) {
  if (!config.whatsapp.enabled || !config.whatsapp.accountSid || !config.whatsapp.authToken) {
    console.log('⚠️  WhatsApp notifications disabled or not configured');
    return { success: false, reason: 'disabled' };
  }

  const results = [];
  
  // Twilio API endpoint
  const url = `https://api.twilio.com/2010-04-01/Accounts/${config.whatsapp.accountSid}/Messages.json`;
  const auth = Buffer.from(`${config.whatsapp.accountSid}:${config.whatsapp.authToken}`).toString('base64');

  for (const toNumber of config.whatsapp.toNumbers) {
    try {
      const response = await axios.post(url, 
        new URLSearchParams({
          From: `whatsapp:${config.whatsapp.fromNumber}`,
          To: `whatsapp:${toNumber.trim()}`,
          Body: message
        }),
        {
          headers: {
            'Authorization': `Basic ${auth}`,
            'Content-Type': 'application/x-www-form-urlencoded'
          },
          timeout: 10000
        }
      );

      if (response.status === 201) {
        console.log(`✅ WhatsApp notification sent to ${toNumber}`);
        results.push({ number: toNumber, success: true, sid: response.data.sid });
      } else {
        console.error(`❌ WhatsApp API error for ${toNumber}:`, response.status);
        results.push({ number: toNumber, success: false, status: response.status });
      }
    } catch (error) {
      console.error(`❌ Failed to send WhatsApp notification to ${toNumber}:`, error.message);
      results.push({ number: toNumber, success: false, error: error.message });
    }
  }

  return {
    success: results.some(r => r.success),
    results
  };
}

/**
 * Format fall detection alert message
 */
function formatFallAlert(deviceId, fallData) {
  const timestamp = new Date().toLocaleString('zh-TW', { timeZone: 'Asia/Taipei' });
  
  // Telegram format (Markdown)
  const telegramMessage = `🚨 *跌倒警報*\n\n` +
    `⏰ 時間：${timestamp}\n` +
    `📱 裝置：\`${deviceId}\`\n` +
    `⚡ 衝擊力：${fallData.jerk_magnitude?.toFixed(2) || 'N/A'} m/s³\n` +
    `📐 傾斜角度：${fallData.pitch_angle?.toFixed(1) || 'N/A'}°\n` +
    `💓 心率：${fallData.heart_rate || 'N/A'} bpm\n` +
    `🌡️ 體溫：${fallData.body_temperature?.toFixed(1) || 'N/A'}°C\n\n` +
    `⚠️ *請立即檢查員工狀況！*`;

  // Plain text format (WhatsApp)
  const whatsappMessage = `🚨 跌倒警報\n\n` +
    `時間：${timestamp}\n` +
    `裝置：${deviceId}\n` +
    `衝擊力：${fallData.jerk_magnitude?.toFixed(2) || 'N/A'} m/s³\n` +
    `傾斜角度：${fallData.pitch_angle?.toFixed(1) || 'N/A'}°\n` +
    `心率：${fallData.heart_rate || 'N/A'} bpm\n` +
    `體溫：${fallData.body_temperature?.toFixed(1) || 'N/A'}°C\n\n` +
    `⚠️ 請立即檢查員工狀況！`;

  // Discord embed format
  const discordTitle = '🚨 跌倒警報';
  const discordDescription = `裝置 **${deviceId}** 偵測到跌倒事件`;
  const discordFields = [
    { name: '⏰ 時間', value: timestamp, inline: true },
    { name: '📱 裝置', value: deviceId, inline: true },
    { name: '⚡ 衝擊力', value: `${fallData.jerk_magnitude?.toFixed(2) || 'N/A'} m/s³`, inline: true },
    { name: '📐 傾斜角度', value: `${fallData.pitch_angle?.toFixed(1) || 'N/A'}°`, inline: true },
    { name: '💓 心率', value: `${fallData.heart_rate || 'N/A'} bpm`, inline: true },
    { name: '🌡️ 體溫', value: `${fallData.body_temperature?.toFixed(1) || 'N/A'}°C`, inline: true }
  ];

  return {
    telegram: telegramMessage,
    whatsapp: whatsappMessage,
    discord: { title: discordTitle, description: discordDescription, fields: discordFields, color: 0xFF0000 }
  };
}

/**
 * Format heart rate abnormality alert
 */
function formatHeartRateAlert(deviceId, heartRate, threshold) {
  const timestamp = new Date().toLocaleString('zh-TW', { timeZone: 'Asia/Taipei' });
  const status = heartRate > threshold.max ? '過高' : '過低';
  
  const telegramMessage = `💓 *心率異常警報*\n\n` +
    `⏰ 時間：${timestamp}\n` +
    `📱 裝置：\`${deviceId}\`\n` +
    `💓 心率：${heartRate} bpm (*${status}*)\n` +
    `📊 正常範圍：${threshold.min}-${threshold.max} bpm\n\n` +
    `⚠️ *請注意員工健康狀況！*`;

  const whatsappMessage = `💓 心率異常警報\n\n` +
    `時間：${timestamp}\n` +
    `裝置：${deviceId}\n` +
    `心率：${heartRate} bpm (${status})\n` +
    `正常範圍：${threshold.min}-${threshold.max} bpm\n\n` +
    `⚠️ 請注意員工健康狀況！`;

  const discordTitle = '💓 心率異常警報';
  const discordDescription = `裝置 **${deviceId}** 偵測到心率異常`;
  const discordFields = [
    { name: '⏰ 時間', value: timestamp, inline: true },
    { name: '📱 裝置', value: deviceId, inline: true },
    { name: '💓 心率', value: `${heartRate} bpm (${status})`, inline: true },
    { name: '📊 正常範圍', value: `${threshold.min}-${threshold.max} bpm`, inline: true }
  ];

  return {
    telegram: telegramMessage,
    whatsapp: whatsappMessage,
    discord: { title: discordTitle, description: discordDescription, fields: discordFields, color: 0xFFA500 }
  };
}

/**
 * Format temperature abnormality alert
 */
function formatTemperatureAlert(deviceId, temperature, threshold) {
  const timestamp = new Date().toLocaleString('zh-TW', { timeZone: 'Asia/Taipei' });
  const status = temperature > threshold.max ? '過高' : '過低';
  
  const telegramMessage = `🌡️ *體溫異常警報*\n\n` +
    `⏰ 時間：${timestamp}\n` +
    `📱 裝置：\`${deviceId}\`\n` +
    `🌡️ 體溫：${temperature.toFixed(1)}°C (*${status}*)\n` +
    `📊 正常範圍：${threshold.min}-${threshold.max}°C\n\n` +
    `⚠️ *請注意員工健康狀況！*`;

  const whatsappMessage = `🌡️ 體溫異常警報\n\n` +
    `時間：${timestamp}\n` +
    `裝置：${deviceId}\n` +
    `體溫：${temperature.toFixed(1)}°C (${status})\n` +
    `正常範圍：${threshold.min}-${threshold.max}°C\n\n` +
    `⚠️ 請注意員工健康狀況！`;

  const discordTitle = '🌡️ 體溫異常警報';
  const discordDescription = `裝置 **${deviceId}** 偵測到體溫異常`;
  const discordFields = [
    { name: '⏰ 時間', value: timestamp, inline: true },
    { name: '📱 裝置', value: deviceId, inline: true },
    { name: '🌡️ 體溫', value: `${temperature.toFixed(1)}°C (${status})`, inline: true },
    { name: '📊 正常範圍', value: `${threshold.min}-${threshold.max}°C`, inline: true }
  ];

  return {
    telegram: telegramMessage,
    whatsapp: whatsappMessage,
    discord: { title: discordTitle, description: discordDescription, fields: discordFields, color: 0xFF4500 }
  };
}

/**
 * Format high noise level alert
 */
function formatNoiseAlert(deviceId, noiseLevel, threshold) {
  const timestamp = new Date().toLocaleString('zh-TW', { timeZone: 'Asia/Taipei' });
  
  const telegramMessage = `🔊 *噪音過高警報*\n\n` +
    `⏰ 時間：${timestamp}\n` +
    `📱 裝置：\`${deviceId}\`\n` +
    `🔊 噪音級別：${noiseLevel}/255\n` +
    `📊 警報閾值：${threshold}\n\n` +
    `⚠️ *請注意工作環境噪音！*`;

  const whatsappMessage = `🔊 噪音過高警報\n\n` +
    `時間：${timestamp}\n` +
    `裝置：${deviceId}\n` +
    `噪音級別：${noiseLevel}/255\n` +
    `警報閾值：${threshold}\n\n` +
    `⚠️ 請注意工作環境噪音！`;

  const discordTitle = '🔊 噪音過高警報';
  const discordDescription = `裝置 **${deviceId}** 偵測到過高噪音`;
  const discordFields = [
    { name: '⏰ 時間', value: timestamp, inline: true },
    { name: '📱 裝置', value: deviceId, inline: true },
    { name: '🔊 噪音級別', value: `${noiseLevel}/255`, inline: true },
    { name: '📊 警報閾值', value: `${threshold}`, inline: true }
  ];

  return {
    telegram: telegramMessage,
    whatsapp: whatsappMessage,
    discord: { title: discordTitle, description: discordDescription, fields: discordFields, color: 0xFFFF00 }
  };
}

/**
 * Send alert notification to all configured channels
 */
async function sendAlert(alertType, deviceId, data) {
  console.log(`\n📢 Sending ${alertType} alert for device ${deviceId}`);
  
  let messages;
  
  switch (alertType) {
    case 'fall':
      messages = formatFallAlert(deviceId, data);
      break;
    case 'heart_rate':
      messages = formatHeartRateAlert(deviceId, data.heartRate, data.threshold);
      break;
    case 'temperature':
      messages = formatTemperatureAlert(deviceId, data.temperature, data.threshold);
      break;
    case 'noise':
      messages = formatNoiseAlert(deviceId, data.noiseLevel, data.threshold);
      break;
    default:
      console.error(`❌ Unknown alert type: ${alertType}`);
      return { success: false, error: 'Unknown alert type' };
  }

  const results = {
    alertType,
    deviceId,
    timestamp: new Date().toISOString(),
    channels: {}
  };

  // Send to all enabled channels in parallel
  const promises = [];

  if (config.telegram.enabled) {
    promises.push(
      sendTelegramNotification(messages.telegram)
        .then(result => { results.channels.telegram = result; })
        .catch(error => { results.channels.telegram = { success: false, error: error.message }; })
    );
  }

  if (config.discord.enabled) {
    promises.push(
      sendDiscordNotification(
        messages.discord.title,
        messages.discord.description,
        messages.discord.color,
        messages.discord.fields
      )
        .then(result => { results.channels.discord = result; })
        .catch(error => { results.channels.discord = { success: false, error: error.message }; })
    );
  }

  if (config.whatsapp.enabled) {
    promises.push(
      sendWhatsAppNotification(messages.whatsapp)
        .then(result => { results.channels.whatsapp = result; })
        .catch(error => { results.channels.whatsapp = { success: false, error: error.message }; })
    );
  }

  await Promise.all(promises);

  // Check if at least one channel succeeded
  results.success = Object.values(results.channels).some(channel => channel.success);

  console.log(`📢 Alert sent: ${results.success ? '✅ Success' : '❌ Failed'}`);
  
  return results;
}

/**
 * Get notification service status
 */
function getStatus() {
  return {
    telegram: {
      enabled: config.telegram.enabled,
      configured: !!(config.telegram.botToken && config.telegram.chatIds.length > 0),
      chatCount: config.telegram.chatIds.length
    },
    discord: {
      enabled: config.discord.enabled,
      configured: config.discord.webhookUrls.length > 0,
      webhookCount: config.discord.webhookUrls.length
    },
    whatsapp: {
      enabled: config.whatsapp.enabled,
      configured: !!(config.whatsapp.accountSid && config.whatsapp.authToken && config.whatsapp.fromNumber),
      recipientCount: config.whatsapp.toNumbers.length
    }
  };
}

module.exports = {
  sendAlert,
  sendTelegramNotification,
  sendDiscordNotification,
  sendWhatsAppNotification,
  getStatus
};
