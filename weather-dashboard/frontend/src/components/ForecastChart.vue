<template>
  <div class="bg-white rounded-xl shadow-lg p-6">
    <div class="flex justify-between items-center mb-6">
      <h3 class="text-lg font-semibold text-gray-900">Weather Forecast</h3>
      <div class="flex space-x-2">
        <button
          v-for="metric in availableMetrics"
          :key="metric.key"
          @click="toggleMetric(metric.key)"
          :class="[
            'px-3 py-1 text-sm rounded-full border transition-colors',
            visibleMetrics[metric.key]
              ? 'bg-blue-500 text-white border-blue-500'
              : 'bg-gray-100 text-gray-600 border-gray-300 hover:bg-gray-200'
          ]"
        >
          {{ metric.label }}
        </button>
      </div>
    </div>
    
    <!-- Debug info panel (can be removed when chart is working) -->
    <!-- <div class="bg-blue-50 border border-blue-200 rounded p-2 mb-4 text-xs">
      <p><strong>Chart Status:</strong></p>
      <p>Forecast items: {{ forecast?.length || 0 }}</p>
      <p>Temperature unit: {{ temperatureUnit }}</p>
      <p>Chart instance: {{ !!chartInstance ? 'Created' : 'Not created' }}</p>
      <p v-if="chartError" class="text-red-600"><strong>Error:</strong> {{ chartError }}</p>
    </div> -->
    
    <div class="relative h-80">
      <canvas ref="chartCanvas"></canvas>
      <div v-if="chartError" class="absolute inset-0 flex items-center justify-center bg-red-50 rounded">
        <div class="text-center text-red-600">
          <p class="font-medium">Chart Error</p>
          <p class="text-sm">{{ chartError }}</p>
        </div>
      </div>
    </div>
    
    <div class="mt-4 flex justify-center space-x-6 text-sm">
      <div v-for="metric in availableMetrics" :key="metric.key" class="flex items-center space-x-2">
        <div 
          :class="['w-3 h-3 rounded-full', metric.colorClass]"
          :style="{ backgroundColor: metric.color }"
        ></div>
        <span class="text-gray-600">{{ metric.label }}</span>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted, onUnmounted, watch, nextTick, computed } from 'vue'
import {
  Chart,
  CategoryScale,
  LinearScale,
  PointElement,
  LineElement,
  LineController,
  BarElement,
  BarController,
  Title,
  Tooltip,
  Legend,
  Filler
} from 'chart.js'
import type { ForecastDaily, TemperatureUnit, WindUnit } from '@/types/weather'

// Register Chart.js components
Chart.register(
  CategoryScale,
  LinearScale,
  PointElement,
  LineElement,
  LineController,
  BarElement,
  BarController,
  Title,
  Tooltip,
  Legend,
  Filler
)

interface Props {
  forecast: ForecastDaily[]
  temperatureUnit: TemperatureUnit
  windUnit: WindUnit
}

const props = defineProps<Props>()

const chartCanvas = ref<HTMLCanvasElement>()
let chartInstance: Chart | null = null
const chartError = ref<string | null>(null)

const availableMetrics = computed(() => [
  { key: 'temperature', label: 'Temperature', color: '#f59e0b', colorClass: 'bg-amber-500' },
  { 
    key: 'wind', 
    label: `Wind Speed (${getWindUnitLabel()})`, 
    color: '#06b6d4', 
    colorClass: 'bg-cyan-500' 
  },
  { key: 'precipitation', label: 'Precipitation', color: '#3b82f6', colorClass: 'bg-blue-500' }
])

const visibleMetrics = ref<Record<string, boolean>>({
  temperature: true,
  wind: true,
  precipitation: true
})

const toggleMetric = (metric: string) => {
  visibleMetrics.value[metric] = !visibleMetrics.value[metric]
  updateChart()
}

const getTemperature = (day: ForecastDaily) => {
  return props.temperatureUnit === 'celsius' ? day.day.avgtemp_c : day.day.avgtemp_f
}

const getMaxTemperature = (day: ForecastDaily) => {
  return props.temperatureUnit === 'celsius' ? day.day.maxtemp_c : day.day.maxtemp_f
}

const getMinTemperature = (day: ForecastDaily) => {
  return props.temperatureUnit === 'celsius' ? day.day.mintemp_c : day.day.mintemp_f
}

const getWindSpeed = (day: ForecastDaily) => {
  // Calculate daytime average wind speed (6 AM to 6 PM)
  if (day.hour && day.hour.length > 0) {
    const daytimeHours = day.hour.filter(hour => {
      const hourTime = new Date(hour.time).getHours()
      return hourTime >= 6 && hourTime <= 18
    })
    
    if (daytimeHours.length > 0) {
      const totalWind = daytimeHours.reduce((sum, hour) => sum + hour.wind_kph, 0)
      const avgWind = totalWind / daytimeHours.length
      return convertWindSpeed(avgWind)
    }
  }
  
  // Fallback to max wind if hourly data not available
  const kph = day.day.maxwind_kph
  return convertWindSpeed(kph)
}

const getDominantWindDirection = (day: ForecastDaily) => {
  // Calculate dominant wind direction during daytime
  if (day.hour && day.hour.length > 0) {
    const daytimeHours = day.hour.filter(hour => {
      const hourTime = new Date(hour.time).getHours()
      return hourTime >= 6 && hourTime <= 18
    })
    
    if (daytimeHours.length > 0) {
      // Count occurrences of each wind direction
      const directionCounts: Record<string, number> = {}
      daytimeHours.forEach(hour => {
        if (hour.wind_dir) {
          directionCounts[hour.wind_dir] = (directionCounts[hour.wind_dir] || 0) + 1
        }
      })
      
      // Find most common direction
      let maxCount = 0
      let dominantDirection = 'Variable'
      for (const [direction, count] of Object.entries(directionCounts)) {
        if (count > maxCount) {
          maxCount = count
          dominantDirection = direction
        }
      }
      
      return dominantDirection
    }
  }
  
  return 'Variable'
}

const convertWindSpeed = (kph: number) => {
  switch (props.windUnit) {
    case 'knots':
      return kph * 0.539957 // Convert km/h to knots
    case 'ms':
      return kph / 3.6 // Convert km/h to m/s
    case 'kmh':
    default:
      return kph
  }
}

const getWindUnitLabel = () => {
  switch (props.windUnit) {
    case 'knots':
      return 'knots'
    case 'ms':
      return 'm/s'
    case 'kmh':
    default:
      return 'km/h'
  }
}

const getPrecipitation = (day: ForecastDaily) => {
  return day.day.totalprecip_mm
}

const formatDate = (dateStr: string) => {
  const date = new Date(dateStr)
  return date.toLocaleDateString('en-US', { weekday: 'short', month: 'short', day: 'numeric' })
}

const createChart = () => {
  chartError.value = null
  
  console.log('createChart called with:', {
    hasCanvas: !!chartCanvas.value,
    forecastLength: props.forecast?.length || 0,
    forecast: props.forecast
  })

  if (!chartCanvas.value || !props.forecast?.length) {
    const errorMsg = `Chart creation skipped: ${!chartCanvas.value ? 'no canvas' : ''} ${!props.forecast?.length ? 'no forecast data' : ''}`
    console.log(errorMsg)
    chartError.value = errorMsg
    return
  }

  const ctx = chartCanvas.value.getContext('2d')
  if (!ctx) {
    const errorMsg = 'Could not get canvas context'
    console.error(errorMsg)
    chartError.value = errorMsg
    return
  }

  // Check canvas size
  const rect = chartCanvas.value.getBoundingClientRect()
  console.log('Canvas size check:', {
    canvasWidth: chartCanvas.value.width,
    canvasHeight: chartCanvas.value.height,
    rectWidth: rect.width,
    rectHeight: rect.height
  })

  if (rect.width === 0 || rect.height === 0) {
    const errorMsg = 'Canvas has zero dimensions'
    console.error(errorMsg)
    chartError.value = errorMsg
    return
  }

  try {
    console.log('Creating chart with forecast data:', props.forecast.length, 'days')

    const labels = props.forecast.map(day => formatDate(day.date))
    
    const datasets: any[] = []

    // Temperature dataset (line)
    if (visibleMetrics.value.temperature) {
      const tempData = props.forecast.map((day, index) => {
        const temp = getTemperature(day)
        if (temp === null || temp === undefined || isNaN(temp)) {
          console.error(`Invalid temperature at day ${index + 1} (${day.date}):`, temp)
        }
        return temp
      })
      
      const maxTempData = props.forecast.map(getMaxTemperature)
      const minTempData = props.forecast.map(getMinTemperature)
      
      // Validate temperature data
      const hasInvalidData = tempData.some((temp) => {
        return temp === null || temp === undefined || isNaN(temp)
      })
      if (hasInvalidData) {
        throw new Error(`Invalid temperature data found: ${JSON.stringify(tempData)}`)
      }

      datasets.push({
        label: `Avg Temperature (°${props.temperatureUnit === 'celsius' ? 'C' : 'F'})`,
        data: tempData,
        borderColor: '#f59e0b',
        backgroundColor: 'rgba(245, 158, 11, 0.1)',
        fill: false,
        type: 'line' as const,
        yAxisID: 'y',
        tension: 0.4,
        pointRadius: 4,
        pointHoverRadius: 6
      })

      datasets.push({
        label: `Max Temperature (°${props.temperatureUnit === 'celsius' ? 'C' : 'F'})`,
        data: maxTempData,
        borderColor: 'rgba(245, 158, 11, 0.7)',
        backgroundColor: 'rgba(245, 158, 11, 0.1)',
        fill: '+1',
        type: 'line' as const,
        yAxisID: 'y',
        pointRadius: 2,
        borderWidth: 1,
        tension: 0.4
      })
      
      datasets.push({
        label: `Min Temperature (°${props.temperatureUnit === 'celsius' ? 'C' : 'F'})`,
        data: minTempData,
        borderColor: 'rgba(245, 158, 11, 0.4)',
        backgroundColor: 'rgba(245, 158, 11, 0.05)',
        fill: false,
        type: 'line' as const,
        yAxisID: 'y',
        pointRadius: 2,
        borderWidth: 1,
        tension: 0.4
      })
    }

    // Wind speed dataset (line)
    if (visibleMetrics.value.wind) {
      const windData = props.forecast.map(getWindSpeed)
      const windDirections = props.forecast.map(getDominantWindDirection)
      
      datasets.push({
        label: `Wind Speed (${getWindUnitLabel()})`,
        data: windData,
        borderColor: '#06b6d4',
        backgroundColor: 'rgba(6, 182, 212, 0.1)',
        fill: false,
        type: 'line' as const,
        yAxisID: 'y1',
        tension: 0.4,
        pointRadius: 6,
        pointHoverRadius: 8,
        pointStyle: 'triangle',
        pointBackgroundColor: '#06b6d4',
        pointBorderColor: '#0891b2',
        pointBorderWidth: 2,
        // Store wind directions for tooltip
        windDirections: windDirections
      })
    }

    // Precipitation dataset (bar)
    if (visibleMetrics.value.precipitation) {
      const precipData = props.forecast.map(getPrecipitation)
      
      datasets.push({
        label: 'Precipitation (mm)',
        data: precipData,
        backgroundColor: 'rgba(59, 130, 246, 0.6)',
        borderColor: '#3b82f6',
        borderWidth: 1,
        type: 'bar' as const,
        yAxisID: 'y2'
      })
    }

    console.log('Chart datasets:', datasets)

    console.log('Creating Chart.js instance...')
    chartInstance = new Chart(ctx, {
      type: 'line',
      data: {
        labels,
        datasets
      },
      options: {
        responsive: true,
        maintainAspectRatio: false,
        interaction: {
          mode: 'index',
          intersect: false
        },
        plugins: {
          title: {
            display: true,
            text: 'Temperature Forecast'
          },
          legend: {
            display: true,
            position: 'top'
          },
          tooltip: {
            backgroundColor: 'rgba(0, 0, 0, 0.8)',
            titleColor: 'white',
            bodyColor: 'white',
            borderColor: 'rgba(255, 255, 255, 0.2)',
            borderWidth: 1,
            callbacks: {
              afterLabel: function(context: any) {
                // Add wind direction information for wind speed data
                if (context.dataset.label && context.dataset.label.includes('Wind Speed')) {
                  const windDirection = getDominantWindDirection(props.forecast[context.dataIndex])
                  return `Direction: ${windDirection}`
                }
                return ''
              }
            }
          }
        },
        scales: {
          x: {
            display: true,
            grid: {
              display: true,
              color: 'rgba(0, 0, 0, 0.1)'
            }
          },
          y: {
            type: 'linear',
            display: visibleMetrics.value.temperature,
            position: 'left',
            title: {
              display: true,
              text: `Temperature (°${props.temperatureUnit === 'celsius' ? 'C' : 'F'})`
            },
            grid: {
              color: 'rgba(0, 0, 0, 0.1)'
            }
          },
          y1: {
            type: 'linear',
            display: visibleMetrics.value.wind,
            position: 'right',
            title: {
              display: true,
              text: `Wind Speed (${getWindUnitLabel()})`
            },
            grid: {
              drawOnChartArea: false
            }
          },
          y2: {
            type: 'linear',
            display: visibleMetrics.value.precipitation,
            position: 'right',
            title: {
              display: true,
              text: 'Precipitation (mm)'
            },
            grid: {
              drawOnChartArea: false
            },
            beginAtZero: true
          }
        }
      }
    })
    
    console.log('Chart created successfully!')
  
  } catch (error) {
    const errorMsg = `Failed to create chart: ${error instanceof Error ? error.message : String(error)}`
    console.error(errorMsg, error)
    chartError.value = errorMsg
  }
}

const updateChart = () => {
  console.log('updateChart called, destroying existing chart and recreating')
  if (chartInstance) {
    chartInstance.destroy()
    chartInstance = null
  }
  nextTick(() => {
    createChart()
  })
}

watch(() => props.forecast, (newForecast, oldForecast) => {
  console.log('Forecast prop changed:', {
    newLength: newForecast?.length || 0,
    oldLength: oldForecast?.length || 0,
    newForecast: newForecast
  })
  updateChart()
}, { deep: true })

watch(() => props.temperatureUnit, (newUnit, oldUnit) => {
  console.log('Temperature unit changed:', { newUnit, oldUnit })
  updateChart()
})

onMounted(() => {
  console.log('ForecastChart mounted, initial forecast:', props.forecast)
  // Wait for the DOM to fully render
  nextTick(() => {
    console.log('DOM updated, attempting to create chart')
    createChart()
  })
})

onUnmounted(() => {
  if (chartInstance) {
    chartInstance.destroy()
  }
})
</script>